#![allow(clippy::match_like_matches_macro)]
/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
use oxc_allocator::Allocator;
use oxc_ast::ast::*;
use oxc_parser::Parser;
use oxc_span::{GetSpan, SourceType, Span};

#[derive(Clone, Copy)]
pub struct InjectArg {
    pub path: &'static [&'static str],
    pub arg: &'static str,
}

pub struct Rename {
    pub path: &'static [&'static str],
    pub replacement: &'static str,
}

pub struct InjectConst {
    pub path: &'static [&'static str],
    pub local_name: &'static str,
    pub init: &'static str,
}

pub struct ConsoleHook {
    pub fn_name: &'static str,
    pub methods: &'static [&'static str],
}

#[derive(Clone)]
struct Patch {
    start: u32,
    end: u32,
    text: String,
}

struct Ctx<'s> {
    source: &'s str,
    inject_args: &'s [InjectArg],
    renames: &'s [Rename],
    inject_consts: &'s [InjectConst],
    console_hook: Option<&'s ConsoleHook>,
    source_map: Option<&'s sourcemap::SourceMap>,
    patches: Vec<Patch>,
    matched_consts: Vec<usize>,
    iife_body_start: Option<u32>,
}

fn offset_to_line_col(source: &str, offset: usize) -> (u32, u32) {
    let bytes = source.as_bytes();
    let safe = offset.min(bytes.len());
    let mut line = 0u32;
    let mut last_nl = 0usize;
    for (i, &b) in bytes[..safe].iter().enumerate() {
        if b == b'\n' {
            line += 1;
            last_nl = i + 1;
        }
    }
    (line, (safe - last_nl) as u32)
}

impl<'s> Ctx<'s> {
    fn new(
        source: &'s str,
        inject_args: &'s [InjectArg],
        renames: &'s [Rename],
        inject_consts: &'s [InjectConst],
        console_hook: Option<&'s ConsoleHook>,
        source_map: Option<&'s sourcemap::SourceMap>,
    ) -> Self {
        Self {
            source,
            inject_args,
            renames,
            inject_consts,
            console_hook,
            source_map,
            patches: Vec::new(),
            matched_consts: Vec::new(),
            iife_body_start: None,
        }
    }

    fn resolve_src_loc(&self, byte_offset: usize) -> (String, String) {
        let sm = match self.source_map {
            Some(sm) => sm,
            None => return ("undefined".to_owned(), "undefined".to_owned()),
        };
        let (line, col) = offset_to_line_col(self.source, byte_offset);
        match sm.lookup_token(line, col) {
            Some(tok) => {
                let file_lit = match tok.get_source() {
                    Some(src) => {
                        let s = src.replace('\\', "/");
                        let base = s.split('/').next_back().unwrap_or(src);
                        serde_json::to_string(base).unwrap_or_else(|_| "undefined".to_owned())
                    }
                    None => "undefined".to_owned(),
                };
                let line_lit = (tok.get_src_line() + 1).to_string(); // 1-indexed
                (file_lit, line_lit)
            }
            None => ("undefined".to_owned(), "undefined".to_owned()),
        }
    }

    fn handle_call(&mut self, call: &CallExpression<'_>) {
        if let Some(hook) = self.console_hook {
            if let Some((level, member_span)) = detect_console_call(&call.callee, hook.methods) {
                self.patches.push(Patch {
                    start: member_span.start,
                    end: member_span.end,
                    text: hook.fn_name.to_string(),
                });
                let callee_end = call.callee.span().end as usize;
                let after = &self.source[callee_end..];
                let paren = after.find('(').expect("CallExpression must have '('");
                let insert_at = (callee_end + paren + 1) as u32;
                let (file_lit, line_lit) = self.resolve_src_loc(call.callee.span().start as usize);
                let text = if call.arguments.is_empty() {
                    format!("\"{}\", {}, {}", level, file_lit, line_lit)
                } else {
                    format!("\"{}\", {}, {}, ", level, file_lit, line_lit)
                };
                self.patches.push(Patch {
                    start: insert_at,
                    end: insert_at,
                    text,
                });
                return;
            }
        }

        for spec in self.inject_args {
            if !callee_matches_path(&call.callee, spec.path) {
                continue;
            }
            if let Some(first) = call.arguments.first() {
                if arg_is_identifier(first, spec.arg) {
                    continue;
                }
            }
            let callee_end = call.callee.span().end as usize;
            let after = &self.source[callee_end..];
            let paren = after.find('(').expect("CallExpression must have '('");
            let insert_at = (callee_end + paren + 1) as u32;
            let text = if call.arguments.is_empty() {
                spec.arg.to_string()
            } else {
                format!("{}, ", spec.arg)
            };
            self.patches.push(Patch {
                start: insert_at,
                end: insert_at,
                text,
            });
        }
    }

    fn handle_static_member(&mut self, member: &StaticMemberExpression<'_>, is_callee: bool) {
        for spec in self.renames {
            if static_member_matches_path(member, spec.path) {
                self.patches.push(Patch {
                    start: member.span.start,
                    end: member.span.end,
                    text: spec.replacement.to_string(),
                });
                return;
            }
        }
        if !is_callee {
            for (i, spec) in self.inject_consts.iter().enumerate() {
                if static_member_matches_path(member, spec.path) {
                    self.patches.push(Patch {
                        start: member.span.start,
                        end: member.span.end,
                        text: spec.local_name.to_string(),
                    });
                    if !self.matched_consts.contains(&i) {
                        self.matched_consts.push(i);
                    }
                    return;
                }
            }
        }
    }
}

fn detect_console_call<'a>(
    callee: &'a Expression<'a>,
    methods: &[&str],
) -> Option<(&'a str, Span)> {
    let inner: &Expression<'a> = match callee {
        Expression::ParenthesizedExpression(p) => &p.expression,
        Expression::SequenceExpression(seq) => match seq.expressions.last() {
            Some(e) => e,
            None => return None,
        },
        other => other,
    };
    if let Expression::StaticMemberExpression(m) = inner {
        if matches!(&m.object, Expression::Identifier(id) if id.name.as_str() == "console") {
            let prop = m.property.name.as_str();
            if methods.contains(&prop) {
                return Some((prop, m.span));
            }
        }
    }
    None
}

fn member_matches_path(expr: &Expression<'_>, path: &[&str]) -> bool {
    match path {
        [] => false,
        [single] => matches!(expr, Expression::Identifier(id) if id.name.as_str() == *single),
        [rest @ .., last] => match expr {
            Expression::StaticMemberExpression(m) => {
                m.property.name.as_str() == *last && member_matches_path(&m.object, rest)
            }
            _ => false,
        },
    }
}

fn static_member_matches_path(member: &StaticMemberExpression<'_>, path: &[&str]) -> bool {
    match path {
        [] | [_] => false,
        [rest @ .., last] => {
            member.property.name.as_str() == *last && member_matches_path(&member.object, rest)
        }
    }
}

fn callee_matches_path(callee: &Expression<'_>, path: &[&str]) -> bool {
    match callee {
        Expression::ParenthesizedExpression(p) => callee_matches_path(&p.expression, path),
        Expression::SequenceExpression(seq) => seq
            .expressions
            .last()
            .is_some_and(|last| member_matches_path(last, path)),
        _ => member_matches_path(callee, path),
    }
}

fn arg_is_identifier(arg: &Argument<'_>, name: &str) -> bool {
    matches!(arg, Argument::Identifier(id) if id.name.as_str() == name)
}

fn walk_program(program: &Program<'_>, ctx: &mut Ctx<'_>) {
    if !ctx.inject_consts.is_empty() && ctx.iife_body_start.is_none() {
        ctx.iife_body_start = find_iife_body_start(program);
    }
    for stmt in &program.body {
        walk_stmt(stmt, ctx);
    }
}

fn find_iife_body_start(program: &Program<'_>) -> Option<u32> {
    for stmt in &program.body {
        let call = match stmt {
            Statement::ExpressionStatement(es) => {
                if let Expression::CallExpression(call) = &es.expression {
                    Some(call.as_ref())
                } else {
                    None
                }
            }
            Statement::VariableDeclaration(vd) => vd.declarations.iter().find_map(|d| {
                d.init.as_ref().and_then(|init| {
                    if let Expression::CallExpression(call) = init {
                        Some(call.as_ref())
                    } else {
                        None
                    }
                })
            }),
            _ => None,
        };
        if let Some(call) = call {
            if let Some(f) = get_func_from_callee(&call.callee) {
                if let Some(body) = &f.body {
                    return Some(body.span.start + 1);
                }
            }
        }
    }
    None
}

fn get_func_from_callee<'a>(callee: &'a Expression<'a>) -> Option<&'a Function<'a>> {
    match callee {
        Expression::FunctionExpression(f) => Some(f),
        Expression::ParenthesizedExpression(p) => get_func_from_callee(&p.expression),
        Expression::SequenceExpression(seq) => {
            seq.expressions.iter().find_map(|e| get_func_from_callee(e))
        }
        _ => None,
    }
}

fn walk_stmt(stmt: &Statement<'_>, ctx: &mut Ctx<'_>) {
    match stmt {
        Statement::ExpressionStatement(es) => walk_expr(&es.expression, ctx),
        Statement::VariableDeclaration(vd) => {
            for d in &vd.declarations {
                if let Some(init) = &d.init {
                    walk_expr(init, ctx);
                }
            }
        }
        Statement::ReturnStatement(rs) => {
            if let Some(arg) = &rs.argument {
                walk_expr(arg, ctx);
            }
        }
        Statement::IfStatement(is) => {
            walk_expr(&is.test, ctx);
            walk_stmt(&is.consequent, ctx);
            if let Some(alt) = &is.alternate {
                walk_stmt(alt, ctx);
            }
        }
        Statement::BlockStatement(bs) => {
            for s in &bs.body {
                walk_stmt(s, ctx);
            }
        }
        Statement::WhileStatement(ws) => {
            walk_expr(&ws.test, ctx);
            walk_stmt(&ws.body, ctx);
        }
        Statement::ForStatement(fs) => {
            if let Some(ForStatementInit::VariableDeclaration(vd)) = &fs.init {
                for d in &vd.declarations {
                    if let Some(init) = &d.init {
                        walk_expr(init, ctx);
                    }
                }
            }
            if let Some(t) = &fs.test {
                walk_expr(t, ctx);
            }
            if let Some(u) = &fs.update {
                walk_expr(u, ctx);
            }
            walk_stmt(&fs.body, ctx);
        }
        Statement::ForInStatement(fs) => {
            walk_expr(&fs.right, ctx);
            walk_stmt(&fs.body, ctx);
        }
        Statement::ForOfStatement(fs) => {
            walk_expr(&fs.right, ctx);
            walk_stmt(&fs.body, ctx);
        }
        Statement::TryStatement(ts) => {
            for s in &ts.block.body {
                walk_stmt(s, ctx);
            }
            if let Some(h) = &ts.handler {
                for s in &h.body.body {
                    walk_stmt(s, ctx);
                }
            }
            if let Some(f) = &ts.finalizer {
                for s in &f.body {
                    walk_stmt(s, ctx);
                }
            }
        }
        Statement::ThrowStatement(ts) => walk_expr(&ts.argument, ctx),
        Statement::SwitchStatement(ss) => {
            walk_expr(&ss.discriminant, ctx);
            for case in &ss.cases {
                if let Some(t) = &case.test {
                    walk_expr(t, ctx);
                }
                for s in &case.consequent {
                    walk_stmt(s, ctx);
                }
            }
        }
        Statement::FunctionDeclaration(f) => {
            if let Some(body) = &f.body {
                walk_fn_body(body, ctx);
            }
        }
        Statement::ClassDeclaration(c) => walk_class(c, ctx),
        _ => {}
    }
}

fn walk_expr(expr: &Expression<'_>, ctx: &mut Ctx<'_>) {
    match expr {
        Expression::CallExpression(call) => {
            ctx.handle_call(call);
            walk_callee(&call.callee, ctx);
            for arg in &call.arguments {
                walk_arg(arg, ctx);
            }
        }
        Expression::StaticMemberExpression(m) => {
            ctx.handle_static_member(m, false);
            walk_expr(&m.object, ctx);
        }
        Expression::SequenceExpression(seq) => {
            for e in &seq.expressions {
                walk_expr(e, ctx);
            }
        }
        Expression::FunctionExpression(f) => {
            if let Some(body) = &f.body {
                walk_fn_body(body, ctx);
            }
        }
        Expression::ArrowFunctionExpression(f) => walk_fn_body(&f.body, ctx),
        Expression::ConditionalExpression(c) => {
            walk_expr(&c.test, ctx);
            walk_expr(&c.consequent, ctx);
            walk_expr(&c.alternate, ctx);
        }
        Expression::AssignmentExpression(a) => walk_expr(&a.right, ctx),
        Expression::LogicalExpression(l) => {
            walk_expr(&l.left, ctx);
            walk_expr(&l.right, ctx);
        }
        Expression::BinaryExpression(b) => {
            walk_expr(&b.left, ctx);
            walk_expr(&b.right, ctx);
        }
        Expression::UnaryExpression(u) => walk_expr(&u.argument, ctx),
        Expression::AwaitExpression(a) => walk_expr(&a.argument, ctx),
        Expression::YieldExpression(y) => {
            if let Some(arg) = &y.argument {
                walk_expr(arg, ctx);
            }
        }
        Expression::NewExpression(n) => {
            walk_expr(&n.callee, ctx);
            for arg in &n.arguments {
                walk_arg(arg, ctx);
            }
        }
        Expression::ObjectExpression(obj) => {
            for prop in &obj.properties {
                match prop {
                    ObjectPropertyKind::ObjectProperty(p) => {
                        walk_expr(&p.value, ctx);
                        if p.computed {
                            walk_property_key(&p.key, ctx);
                        }
                    }
                    ObjectPropertyKind::SpreadProperty(s) => walk_expr(&s.argument, ctx),
                }
            }
        }
        Expression::ArrayExpression(arr) => {
            for elem in &arr.elements {
                match elem {
                    ArrayExpressionElement::SpreadElement(s) => walk_expr(&s.argument, ctx),
                    ArrayExpressionElement::Elision(_) => {}
                    e => walk_array_elem(e, ctx),
                }
            }
        }
        Expression::TemplateLiteral(t) => {
            for e in &t.expressions {
                walk_expr(e, ctx);
            }
        }
        Expression::TaggedTemplateExpression(t) => {
            walk_expr(&t.tag, ctx);
            for e in &t.quasi.expressions {
                walk_expr(e, ctx);
            }
        }
        Expression::ParenthesizedExpression(p) => walk_expr(&p.expression, ctx),
        Expression::ChainExpression(c) => walk_chain_elem(&c.expression, ctx),
        Expression::ClassExpression(c) => walk_class(c, ctx),
        _ => {} // literals, identifiers — leaves
    }
}

fn walk_callee(callee: &Expression<'_>, ctx: &mut Ctx<'_>) {
    match callee {
        Expression::ParenthesizedExpression(p) => walk_callee(&p.expression, ctx),
        Expression::StaticMemberExpression(m) => {
            ctx.handle_static_member(m, true);
            walk_expr(&m.object, ctx);
        }
        Expression::SequenceExpression(seq) => {
            let last = seq.expressions.len().saturating_sub(1);
            for (i, e) in seq.expressions.iter().enumerate() {
                if i == last {
                    walk_callee(e, ctx);
                } else {
                    walk_expr(e, ctx);
                }
            }
        }
        _ => walk_expr(callee, ctx),
    }
}

fn walk_arg(arg: &Argument<'_>, ctx: &mut Ctx<'_>) {
    match arg {
        Argument::SpreadElement(s) => walk_expr(&s.argument, ctx),
        Argument::CallExpression(call) => {
            ctx.handle_call(call);
            walk_callee(&call.callee, ctx);
            for a in &call.arguments {
                walk_arg(a, ctx);
            }
        }
        Argument::StaticMemberExpression(m) => {
            ctx.handle_static_member(m, false);
            walk_expr(&m.object, ctx);
        }
        Argument::SequenceExpression(seq) => {
            for e in &seq.expressions {
                walk_expr(e, ctx);
            }
        }
        Argument::FunctionExpression(f) => {
            if let Some(body) = &f.body {
                walk_fn_body(body, ctx);
            }
        }
        Argument::ArrowFunctionExpression(f) => walk_fn_body(&f.body, ctx),
        Argument::ConditionalExpression(c) => {
            walk_expr(&c.test, ctx);
            walk_expr(&c.consequent, ctx);
            walk_expr(&c.alternate, ctx);
        }
        Argument::AssignmentExpression(a) => walk_expr(&a.right, ctx),
        Argument::LogicalExpression(l) => {
            walk_expr(&l.left, ctx);
            walk_expr(&l.right, ctx);
        }
        Argument::BinaryExpression(b) => {
            walk_expr(&b.left, ctx);
            walk_expr(&b.right, ctx);
        }
        Argument::UnaryExpression(u) => walk_expr(&u.argument, ctx),
        Argument::AwaitExpression(a) => walk_expr(&a.argument, ctx),
        Argument::NewExpression(n) => {
            walk_expr(&n.callee, ctx);
            for a in &n.arguments {
                walk_arg(a, ctx);
            }
        }
        Argument::ObjectExpression(obj) => {
            for prop in &obj.properties {
                match prop {
                    ObjectPropertyKind::ObjectProperty(p) => {
                        walk_expr(&p.value, ctx);
                        if p.computed {
                            walk_property_key(&p.key, ctx);
                        }
                    }
                    ObjectPropertyKind::SpreadProperty(s) => walk_expr(&s.argument, ctx),
                }
            }
        }
        Argument::ArrayExpression(arr) => {
            for elem in &arr.elements {
                match elem {
                    ArrayExpressionElement::SpreadElement(s) => walk_expr(&s.argument, ctx),
                    ArrayExpressionElement::Elision(_) => {}
                    e => walk_array_elem(e, ctx),
                }
            }
        }
        Argument::TemplateLiteral(t) => {
            for e in &t.expressions {
                walk_expr(e, ctx);
            }
        }
        Argument::ParenthesizedExpression(p) => walk_expr(&p.expression, ctx),
        _ => {}
    }
}

fn walk_fn_body(body: &FunctionBody<'_>, ctx: &mut Ctx<'_>) {
    for stmt in &body.statements {
        walk_stmt(stmt, ctx);
    }
}

fn walk_chain_elem(elem: &ChainElement<'_>, ctx: &mut Ctx<'_>) {
    match elem {
        ChainElement::CallExpression(call) => {
            ctx.handle_call(call);
            walk_callee(&call.callee, ctx);
            for arg in &call.arguments {
                walk_arg(arg, ctx);
            }
        }
        ChainElement::StaticMemberExpression(m) => {
            ctx.handle_static_member(m, false);
            walk_expr(&m.object, ctx);
        }
        ChainElement::ComputedMemberExpression(m) => {
            walk_expr(&m.object, ctx);
            walk_expr(&m.expression, ctx);
        }
        _ => {}
    }
}

fn walk_property_key(key: &PropertyKey<'_>, ctx: &mut Ctx<'_>) {
    match key {
        PropertyKey::StaticMemberExpression(m) => {
            ctx.handle_static_member(m, false);
            walk_expr(&m.object, ctx);
        }
        PropertyKey::CallExpression(call) => {
            ctx.handle_call(call);
            walk_callee(&call.callee, ctx);
            for arg in &call.arguments {
                walk_arg(arg, ctx);
            }
        }
        _ => {}
    }
}

fn walk_array_elem(elem: &ArrayExpressionElement<'_>, ctx: &mut Ctx<'_>) {
    match elem {
        ArrayExpressionElement::SpreadElement(s) => walk_expr(&s.argument, ctx),
        ArrayExpressionElement::Elision(_) => {}
        ArrayExpressionElement::CallExpression(c) => {
            ctx.handle_call(c);
            walk_callee(&c.callee, ctx);
            for a in &c.arguments {
                walk_arg(a, ctx);
            }
        }
        ArrayExpressionElement::StaticMemberExpression(m) => {
            ctx.handle_static_member(m, false);
            walk_expr(&m.object, ctx);
        }
        ArrayExpressionElement::SequenceExpression(s) => {
            for e in &s.expressions {
                walk_expr(e, ctx);
            }
        }
        ArrayExpressionElement::FunctionExpression(f) => {
            if let Some(body) = &f.body {
                walk_fn_body(body, ctx);
            }
        }
        ArrayExpressionElement::ArrowFunctionExpression(f) => walk_fn_body(&f.body, ctx),
        _ => {}
    }
}

fn walk_class(class: &Class<'_>, ctx: &mut Ctx<'_>) {
    if let Some(sc) = &class.super_class {
        walk_expr(sc, ctx);
    }
    for item in &class.body.body {
        match item {
            ClassElement::MethodDefinition(m) => {
                if let Some(body) = &m.value.body {
                    walk_fn_body(body, ctx);
                }
            }
            ClassElement::PropertyDefinition(p) => {
                if let Some(val) = &p.value {
                    walk_expr(val, ctx);
                }
            }
            ClassElement::StaticBlock(sb) => {
                for s in &sb.body {
                    walk_stmt(s, ctx);
                }
            }
            _ => {}
        }
    }
}

fn apply_patches_inner(source: &str, mut patches: Vec<Patch>) -> String {
    patches.sort_by(|a, b| b.start.cmp(&a.start).then(b.end.cmp(&a.end)));
    let mut result = source.to_string();
    for p in patches {
        result.replace_range(p.start as usize..p.end as usize, &p.text);
    }
    result
}

pub static CONSOLE_HOOK: ConsoleHook = ConsoleHook {
    fn_name: "__millennium_plugin_console__",
    methods: &["log", "warn", "error"],
};

pub fn apply_transforms(
    source: &str,
    inject_args: &[InjectArg],
    renames: &[Rename],
    inject_consts: &[InjectConst],
    console_hook: Option<&ConsoleHook>,
    source_map: Option<&sourcemap::SourceMap>,
) -> String {
    let alloc = Allocator::default();
    let ret = Parser::new(&alloc, source, SourceType::default()).parse();
    if ret.panicked || !ret.errors.is_empty() {
        crate::log::warn("transforms: parse error(s), applying without transforms");
        return source.to_string();
    }

    let mut ctx = Ctx::new(
        source,
        inject_args,
        renames,
        inject_consts,
        console_hook,
        source_map,
    );
    walk_program(&ret.program, &mut ctx);

    if !ctx.matched_consts.is_empty() {
        if let Some(pos) = ctx.iife_body_start {
            for &i in &ctx.matched_consts {
                let ic = &inject_consts[i];
                let decl = format!("\nconst {} = {};\n", ic.local_name, ic.init);
                ctx.patches.push(Patch {
                    start: pos,
                    end: pos,
                    text: decl,
                });
            }
        }
    }

    apply_patches_inner(source, ctx.patches)
}

macro_rules! inject_args_for_ns {
    ($ns:expr) => {
        [
            InjectArg { path: &[$ns, "callable"], arg: "pluginName" },
            InjectArg { path: &[$ns, "ffi"], arg: "pluginName" },
            InjectArg { path: &[$ns, "Millennium", "callServerMethod"], arg: "pluginName" },
            InjectArg { path: &[$ns, "Millennium", "exposeObj"], arg: "__exports" },
            InjectArg { path: &[$ns, "BindPluginSettings"], arg: "pluginName" },
            InjectArg { path: &[$ns, "pluginConfig", "get"], arg: "pluginName" },
            InjectArg { path: &[$ns, "pluginConfig", "set"], arg: "pluginName" },
            InjectArg { path: &[$ns, "pluginConfig", "delete"], arg: "pluginName" },
            InjectArg { path: &[$ns, "pluginConfig", "getAll"], arg: "pluginName" },
            InjectArg { path: &[$ns, "usePluginConfig"], arg: "pluginName" },
            InjectArg { path: &[$ns, "subscribePluginConfig"], arg: "pluginName" },
        ]
    };
}

static FRONTEND_INJECT_ARGS_CLIENT: &[InjectArg] = &inject_args_for_ns!("_steambrew_client");
static FRONTEND_INJECT_ARGS_MILLENNIUM: &[InjectArg] = &inject_args_for_ns!("_steambrew_millennium");

pub const FRONTEND_INJECT_ARGS_SLICES: &[&[InjectArg]] =
    &[FRONTEND_INJECT_ARGS_CLIENT, FRONTEND_INJECT_ARGS_MILLENNIUM];

pub static FRONTEND_RENAMES: &[Rename] = &[
    Rename {
        path: &["_steamclienthomebrew_millennium", "pluginSelf"],
        replacement: "window.PLUGIN_LIST[pluginName]",
    },
    Rename {
        path: &["_steambrew_client", "pluginSelf"],
        replacement: "window.PLUGIN_LIST[pluginName]",
    },
    Rename {
        path: &["_steambrew_millennium", "pluginSelf"],
        replacement: "window.PLUGIN_LIST[pluginName]",
    },
];

pub static FRONTEND_INJECT_CONSTS: &[InjectConst] = &[
    InjectConst {
        path: &["_steambrew_client", "ChromeDevToolsProtocol"],
        local_name: "ChromeDevToolsProtocol",
        init: "_steambrew_client.MillenniumChromeDevToolsProtocol \
                     ? new _steambrew_client.MillenniumChromeDevToolsProtocol(pluginName) \
                     : _steambrew_client.ChromeDevToolsProtocol",
    },
    InjectConst {
        path: &["_steambrew_millennium", "ChromeDevToolsProtocol"],
        local_name: "ChromeDevToolsProtocol",
        init: "_steambrew_millennium.MillenniumChromeDevToolsProtocol \
                     ? new _steambrew_millennium.MillenniumChromeDevToolsProtocol(pluginName) \
                     : _steambrew_millennium.ChromeDevToolsProtocol",
    },
];

static WEBKIT_INJECT_ARGS_WEBKIT: &[InjectArg] = &inject_args_for_ns!("_steambrew_webkit");
static WEBKIT_INJECT_ARGS_MILLENNIUM: &[InjectArg] = &inject_args_for_ns!("_steambrew_millennium");

pub const WEBKIT_INJECT_ARGS_SLICES: &[&[InjectArg]] =
    &[WEBKIT_INJECT_ARGS_WEBKIT, WEBKIT_INJECT_ARGS_MILLENNIUM];

pub static WEBKIT_RENAMES: &[Rename] = &[Rename {
    path: &["_steambrew_millennium", "pluginSelf"],
    replacement: "window.PLUGIN_LIST[pluginName]",
}];
pub static WEBKIT_INJECT_CONSTS: &[InjectConst] = &[InjectConst {
    path: &["_steambrew_millennium", "ChromeDevToolsProtocol"],
    local_name: "ChromeDevToolsProtocol",
    init: "_steambrew_millennium.MillenniumChromeDevToolsProtocol \
                     ? new _steambrew_millennium.MillenniumChromeDevToolsProtocol(pluginName) \
                     : _steambrew_millennium.ChromeDevToolsProtocol",
}];
