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
use crate::ffi_types::{ffi_type_from_ts, FfiType};
use oxc_allocator::Allocator;
use oxc_ast::ast::*;
use oxc_parser::Parser;
use oxc_span::{GetSpan, SourceType};
use std::path::Path;

#[derive(Debug)]
pub struct TsFfiCall {
    pub fn_name: String,
    pub param_types: Vec<FfiType>,
    pub return_type: FfiType,
    pub file: String,
    pub line: u32,
}

#[derive(Debug)]
#[allow(dead_code)]
pub struct TsExposedFn {
    pub name: String,
    pub params: Vec<(String, FfiType)>,
    pub return_type: FfiType,
    pub file: String,
    pub line: u32,
}

pub struct TsScanResult {
    pub ffi_calls: Vec<TsFfiCall>,
    pub exposed_fns: Vec<TsExposedFn>,
}

pub fn scan(config_dir: &Path, frontend_entry: Option<&str>) -> anyhow::Result<TsScanResult> {
    let mut ffi_calls = Vec::new();
    let mut exposed_fns = Vec::new();

    for path in find_ts_files(config_dir, frontend_entry) {
        let source = match std::fs::read_to_string(&path) {
            Ok(s) => s,
            Err(_) => continue,
        };
        let rel = path
            .strip_prefix(config_dir)
            .unwrap_or(&path)
            .to_string_lossy()
            .to_string();

        let source_type = if path.extension().map_or(false, |e| e == "tsx") {
            SourceType::tsx()
        } else {
            SourceType::ts()
        };

        let alloc = Allocator::default();
        let ret = Parser::new(&alloc, &source, source_type).parse();
        if ret.panicked {
            continue;
        }

        let mut ctx = ScanCtx {
            source: &source,
            file: &rel,
            ffi_calls: &mut ffi_calls,
            exposed_fns: &mut exposed_fns,
        };

        for stmt in &ret.program.body {
            walk_stmt(stmt, &mut ctx);
        }
    }

    Ok(TsScanResult {
        ffi_calls,
        exposed_fns,
    })
}

struct ScanCtx<'a> {
    source: &'a str,
    file: &'a str,
    ffi_calls: &'a mut Vec<TsFfiCall>,
    exposed_fns: &'a mut Vec<TsExposedFn>,
}

fn walk_stmt<'a>(stmt: &Statement<'a>, ctx: &mut ScanCtx<'_>) {
    match stmt {
        Statement::ExpressionStatement(es) => walk_expr(&es.expression, ctx),
        Statement::VariableDeclaration(vd) => {
            for decl in &vd.declarations {
                // Check for /** @ffi */ const name = (params): Ret => ...
                if let Some(init) = &decl.init {
                    if let Some(name) = binding_name(&decl.id) {
                        if has_ffi_comment(ctx.source, decl.id.span().start) {
                            if let Some(exposed) = extract_arrow_ffi(&name, init, ctx.file) {
                                ctx.exposed_fns.push(exposed);
                                return;
                            }
                        }
                    }
                    walk_expr(init, ctx);
                }
            }
        }
        Statement::FunctionDeclaration(fd) => {
            let start = fd.span().start;
            if has_ffi_comment(ctx.source, start) {
                if let Some(exposed) = extract_fn_decl_ffi(fd, ctx.file) {
                    ctx.exposed_fns.push(exposed);
                    return;
                }
            }
            if let Some(body) = &fd.body {
                for s in &body.statements {
                    walk_stmt(s, ctx);
                }
            }
        }
        Statement::ExportNamedDeclaration(exp) => {
            if let Some(Declaration::FunctionDeclaration(fd)) = &exp.declaration {
                let start = exp.span().start;
                if has_ffi_comment(ctx.source, start) {
                    if let Some(exposed) = extract_fn_decl_ffi(fd, ctx.file) {
                        ctx.exposed_fns.push(exposed);
                        return;
                    }
                }
            }
        }
        Statement::BlockStatement(bs) => {
            for s in &bs.body {
                walk_stmt(s, ctx);
            }
        }
        Statement::ReturnStatement(rs) => {
            if let Some(arg) = &rs.argument {
                walk_expr(arg, ctx);
            }
        }
        _ => {}
    }
}

fn walk_expr<'a>(expr: &Expression<'a>, ctx: &mut ScanCtx<'_>) {
    match expr {
        Expression::CallExpression(call) => {
            if let Some(ffi_call) = extract_ffi_call(call, ctx.source, ctx.file) {
                ctx.ffi_calls.push(ffi_call);
            }
            walk_expr(&call.callee, ctx);
            for arg in &call.arguments {
                walk_arg(arg, ctx);
            }
        }
        Expression::ArrowFunctionExpression(arrow) => walk_fn_body(&arrow.body, ctx),
        Expression::FunctionExpression(fe) => {
            if let Some(body) = &fe.body {
                for s in &body.statements {
                    walk_stmt(s, ctx);
                }
            }
        }
        Expression::StaticMemberExpression(m) => walk_expr(&m.object, ctx),
        Expression::AssignmentExpression(a) => walk_expr(&a.right, ctx),
        Expression::LogicalExpression(l) => {
            walk_expr(&l.left, ctx);
            walk_expr(&l.right, ctx);
        }
        Expression::ConditionalExpression(c) => {
            walk_expr(&c.test, ctx);
            walk_expr(&c.consequent, ctx);
            walk_expr(&c.alternate, ctx);
        }
        Expression::SequenceExpression(seq) => {
            for e in &seq.expressions {
                walk_expr(e, ctx);
            }
        }
        Expression::AwaitExpression(a) => walk_expr(&a.argument, ctx),
        _ => {}
    }
}

fn walk_arg<'a>(arg: &Argument<'a>, ctx: &mut ScanCtx<'_>) {
    match arg {
        Argument::CallExpression(call) => {
            if let Some(ffi_call) = extract_ffi_call(call, ctx.source, ctx.file) {
                ctx.ffi_calls.push(ffi_call);
            }
        }
        Argument::ArrowFunctionExpression(f) => walk_fn_body(&f.body, ctx),
        Argument::FunctionExpression(f) => {
            if let Some(body) = &f.body {
                for s in &body.statements {
                    walk_stmt(s, ctx);
                }
            }
        }
        _ => {}
    }
}

fn walk_fn_body<'a>(body: &FunctionBody<'a>, ctx: &mut ScanCtx<'_>) {
    for stmt in &body.statements {
        walk_stmt(stmt, ctx);
    }
}

fn extract_ffi_call<'a>(call: &CallExpression<'a>, source: &str, file: &str) -> Option<TsFfiCall> {
    let is_ffi = match &call.callee {
        Expression::Identifier(id) => matches!(id.name.as_str(), "ffi" | "callable"),
        Expression::StaticMemberExpression(m) => {
            matches!(m.property.name.as_str(), "ffi" | "callable")
        }
        _ => false,
    };
    if !is_ffi {
        return None;
    }

    let fn_name = match call.arguments.first() {
        Some(Argument::StringLiteral(lit)) => lit.value.as_str().to_string(),
        _ => return None,
    };

    let (param_types, return_type) = match &call.type_arguments {
        Some(tp) if !tp.params.is_empty() => {
            let params = extract_tuple_types(&tp.params[0]);
            let ret = tp
                .params
                .get(1)
                .map(ffi_type_from_ts)
                .unwrap_or(FfiType::Unknown);
            (params, ret)
        }
        _ => (Vec::new(), FfiType::Unknown),
    };

    let line = byte_offset_to_line(source, call.span.start as usize);

    Some(TsFfiCall {
        fn_name,
        param_types,
        return_type,
        file: file.to_string(),
        line,
    })
}

fn ffi_type_from_tuple_elem(elem: &TSTupleElement) -> FfiType {
    match elem {
        TSTupleElement::TSNamedTupleMember(m) => ffi_type_from_tuple_elem(&m.element_type),
        TSTupleElement::TSRestType(_) | TSTupleElement::TSOptionalType(_) => FfiType::Unknown,
        TSTupleElement::TSNumberKeyword(_) => FfiType::Number,
        TSTupleElement::TSStringKeyword(_) => FfiType::String,
        TSTupleElement::TSBooleanKeyword(_) => FfiType::Boolean,
        TSTupleElement::TSNullKeyword(_) | TSTupleElement::TSUndefinedKeyword(_) => FfiType::Nil,
        TSTupleElement::TSAnyKeyword(_) | TSTupleElement::TSUnknownKeyword(_) => FfiType::Unknown,
        TSTupleElement::TSVoidKeyword(_) => FfiType::Void,
        TSTupleElement::TSArrayType(t) => {
            FfiType::Array(Box::new(ffi_type_from_ts(&t.element_type)))
        }
        TSTupleElement::TSUnionType(t) => {
            FfiType::Union(t.types.iter().map(ffi_type_from_ts).collect())
        }
        _ => FfiType::Unknown,
    }
}

fn extract_tuple_types(ty: &TSType) -> Vec<FfiType> {
    match ty {
        TSType::TSTupleType(tuple) => tuple
            .element_types
            .iter()
            .map(ffi_type_from_tuple_elem)
            .collect(),
        _ => Vec::new(),
    }
}

fn extract_fn_decl_ffi<'a>(fd: &Function<'a>, file: &str) -> Option<TsExposedFn> {
    let name = fd.id.as_ref()?.name.as_str().to_string();
    let (params, return_type) = extract_fn_signature(fd, file)?;
    Some(TsExposedFn {
        name,
        params,
        return_type,
        file: file.to_string(),
        line: 0,
    })
}

fn extract_arrow_ffi<'a>(name: &str, expr: &Expression<'a>, file: &str) -> Option<TsExposedFn> {
    let arrow = match expr {
        Expression::ArrowFunctionExpression(a) => a,
        _ => return None,
    };

    let params = extract_params(&arrow.params, file)?;
    let return_type = arrow
        .return_type
        .as_ref()
        .map(|r| ffi_type_from_ts(&r.type_annotation))
        .unwrap_or(FfiType::Unknown);

    if matches!(return_type, FfiType::Unknown) {
        crate::log::warn(&format!(
            "FFI: `{}` in {} needs an explicit return type annotation",
            name, file
        ));
    }

    Some(TsExposedFn {
        name: name.to_string(),
        params,
        return_type,
        file: file.to_string(),
        line: 0,
    })
}

fn extract_fn_signature<'a>(
    fd: &Function<'a>,
    file: &str,
) -> Option<(Vec<(String, FfiType)>, FfiType)> {
    let name = fd
        .id
        .as_ref()
        .map(|id| id.name.as_str())
        .unwrap_or("(anonymous)");
    let params = extract_params(&fd.params, file)?;
    let return_type = fd
        .return_type
        .as_ref()
        .map(|r| ffi_type_from_ts(&r.type_annotation))
        .unwrap_or(FfiType::Unknown);

    if matches!(return_type, FfiType::Unknown) {
        crate::log::warn(&format!(
            "FFI: `{}` in {} needs an explicit return type annotation",
            name, file
        ));
    }

    Some((params, return_type))
}

fn extract_params<'a>(params: &FormalParameters<'a>, file: &str) -> Option<Vec<(String, FfiType)>> {
    let mut result = Vec::new();
    for param in &params.items {
        let name = binding_name(&param.pattern).unwrap_or_else(|| "_".to_string());
        let ty = param
            .type_annotation
            .as_ref()
            .map(|ann| ffi_type_from_ts(&ann.type_annotation))
            .unwrap_or(FfiType::Unknown);

        if matches!(ty, FfiType::Unknown) {
            crate::log::warn(&format!(
                "FFI: parameter `{}` in {} needs an explicit type annotation",
                name, file
            ));
        }

        result.push((name, ty));
    }
    Some(result)
}

fn binding_name(pat: &BindingPattern) -> Option<String> {
    match pat {
        BindingPattern::BindingIdentifier(id) => Some(id.name.as_str().to_string()),
        _ => None,
    }
}

fn has_ffi_comment(source: &str, pos: u32) -> bool {
    let before = &source[..pos as usize];
    if let Some(i) = before.rfind("/** @ffi */") {
        let after_comment = &before[i + 11..];
        after_comment.chars().all(|c| c.is_whitespace())
    } else {
        false
    }
}

fn byte_offset_to_line(source: &str, offset: usize) -> u32 {
    source[..offset.min(source.len())]
        .chars()
        .filter(|&c| c == '\n')
        .count() as u32
        + 1
}

fn find_ts_files(config_dir: &Path, frontend_entry: Option<&str>) -> Vec<std::path::PathBuf> {
    // scope to the frontend entry's parent directory so webkit/preload files
    // (which have different ffi semantics) don't get mixed in.
    let scan_root = frontend_entry
        .and_then(|e| std::path::Path::new(e).parent())
        .map(|p| config_dir.join(p))
        .unwrap_or_else(|| config_dir.to_path_buf());

    let mut files = Vec::new();
    for pattern in &["**/*.ts", "**/*.tsx"] {
        let full = scan_root.join(pattern).to_string_lossy().to_string();
        if let Ok(paths) = glob::glob(&full) {
            for p in paths.flatten() {
                let rel = p.strip_prefix(config_dir).unwrap_or(&p);
                let rel_str = rel.to_string_lossy();
                if !rel_str.starts_with(".millennium")
                    && !rel_str.contains("node_modules")
                    && !rel_str.contains(".d.ts")
                {
                    files.push(p);
                }
            }
        }
    }
    files
}
