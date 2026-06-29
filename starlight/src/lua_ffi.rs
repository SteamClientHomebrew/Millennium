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
use crate::ffi_types::{parse_lua_type, FfiType};
use crate::format::section::SubEntry;

#[derive(Debug)]
pub struct LuaExportedFn {
    pub name: String,
    pub params: Vec<(String, FfiType)>,
    pub return_type: Option<FfiType>,
    pub file: String,
    pub line: u32,
}

#[derive(Debug)]
pub struct LuaFrontendCall {
    pub fn_name: String,
    pub file: String,
    pub line: u32,
}

pub struct LuaScanResult {
    pub exported_fns: Vec<LuaExportedFn>,
    pub frontend_calls: Vec<LuaFrontendCall>,
}

pub fn scan(entries: &[SubEntry]) -> anyhow::Result<LuaScanResult> {
    let mut exported_fns = Vec::new();
    let mut frontend_calls = Vec::new();

    for entry in entries {
        let source = match std::str::from_utf8(&entry.data) {
            Ok(s) => s,
            Err(_) => continue,
        };
        let (mut fns, mut calls) = scan_source(source, &entry.name)?;
        exported_fns.append(&mut fns);
        frontend_calls.append(&mut calls);
    }

    Ok(LuaScanResult {
        exported_fns,
        frontend_calls,
    })
}

fn scan_source(
    source: &str,
    file: &str,
) -> anyhow::Result<(Vec<LuaExportedFn>, Vec<LuaFrontendCall>)> {
    let ast = full_moon::parse(source)
        .map_err(|e| anyhow::anyhow!("Lua parse error in {}: {:?}", file, e))?;

    let mut exported_fns = Vec::new();

    for stmt in ast.nodes().stmts() {
        use full_moon::ast::Stmt;
        if let Stmt::FunctionDeclaration(fd) = stmt {
            if let Some(export) = extract_ffi_function(fd, file) {
                exported_fns.push(export);
            }
        }
    }

    let frontend_calls = scan_frontend_calls(source, file);

    Ok((exported_fns, frontend_calls))
}

fn extract_ffi_function(
    fd: &full_moon::ast::FunctionDeclaration,
    file: &str,
) -> Option<LuaExportedFn> {
    use full_moon::tokenizer::TokenType;

    // only simple globals: `function Foo(...)`, not `function a.b(...)` or `function a:b(...)`
    let name_parts: Vec<_> = fd.name().names().iter().map(|p| p.to_string()).collect();
    if name_parts.len() != 1 || fd.name().method_name().is_some() {
        return None;
    }
    let fn_name = name_parts[0].trim().to_string();

    // parse leading trivia on the `function` keyword for LuaDoc annotations
    let mut is_ffi = false;
    let mut params: Vec<(String, FfiType)> = Vec::new();
    let mut return_type: Option<FfiType> = None;

    for trivia in fd.function_token().leading_trivia() {
        match trivia.token_type() {
            TokenType::SingleLineComment { comment } => {
                let content = comment.trim_start_matches('-').trim();

                if content == "@ffi" {
                    is_ffi = true;
                } else if let Some(rest) = content.strip_prefix("@param ") {
                    let mut parts = rest.splitn(3, ' ');
                    let name = parts.next().unwrap_or("_").to_string();
                    let type_str = parts.next().unwrap_or("unknown");
                    params.push((name, parse_lua_type(type_str)));
                } else if let Some(rest) = content.strip_prefix("@return ") {
                    let type_str = rest.split_whitespace().next().unwrap_or("unknown");
                    return_type = Some(parse_lua_type(type_str));
                }
            }
            TokenType::Whitespace { .. } => {}
            _ => {
                // any non-comment, non-whitespace trivia resets the annotation block
                if !is_ffi {
                    params.clear();
                    return_type = None;
                }
            }
        }
    }

    if !is_ffi {
        return None;
    }

    let line = fd.function_token().token().start_position().line() as u32;

    // validate: @ffi requires @return
    if return_type.is_none() {
        crate::log::warn(&format!(
            "FFI: `{}` in {} is missing `---@return` annotation — type will be `unknown`",
            fn_name, file
        ));
    }

    // validate: @param count should match actual param count
    let actual_param_count = fd
        .body()
        .parameters()
        .iter()
        .filter(|p| matches!(**p, full_moon::ast::Parameter::Name(_)))
        .count();

    if params.len() != actual_param_count {
        crate::log::warn(&format!(
            "FFI: `{}` in {} has {} `---@param` annotation(s) but {} actual parameter(s)",
            fn_name,
            file,
            params.len(),
            actual_param_count
        ));
    }

    Some(LuaExportedFn {
        name: fn_name,
        params,
        return_type,
        file: file.to_string(),
        line,
    })
}

fn scan_frontend_calls(source: &str, file: &str) -> Vec<LuaFrontendCall> {
    let mut calls = Vec::new();
    let needle = "call_frontend_method(";

    let mut search_from = 0;
    while let Some(rel) = source[search_from..].find(needle) {
        let abs = search_from + rel + needle.len();
        let after = source[abs..].trim_start();

        let quote = match after.chars().next() {
            Some(q @ ('"' | '\'')) => q,
            _ => {
                search_from = abs;
                continue;
            }
        };

        let rest = &after[1..];
        if let Some(end) = rest.find(quote) {
            let fn_name = rest[..end].to_string();
            let line = source[..search_from + rel]
                .chars()
                .filter(|&c| c == '\n')
                .count() as u32
                + 1;
            calls.push(LuaFrontendCall {
                fn_name,
                file: file.to_string(),
                line,
            });
        }

        search_from = abs;
    }

    calls
}
