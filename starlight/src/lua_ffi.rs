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
use std::collections::{HashMap, HashSet};

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

#[derive(Debug)]
pub struct LuaClassDef {
    pub name: String,
    pub fields: Vec<(String, FfiType, bool)>,
}

pub struct LuaScanResult {
    pub exported_fns: Vec<LuaExportedFn>,
    pub frontend_calls: Vec<LuaFrontendCall>,
    pub classes: Vec<LuaClassDef>,
}

pub fn scan(entries: &[SubEntry]) -> anyhow::Result<LuaScanResult> {
    let mut raw_classes: HashMap<String, Vec<(String, String, bool)>> = HashMap::new();
    for entry in entries {
        if let Ok(source) = std::str::from_utf8(&entry.data) {
            collect_raw_classes(source, &mut raw_classes);
        }
    }

    let class_names: HashSet<String> = raw_classes.keys().cloned().collect();

    let mut classes: Vec<LuaClassDef> = raw_classes
        .into_iter()
        .map(|(name, raw_fields)| LuaClassDef {
            name,
            fields: raw_fields
                .into_iter()
                .map(|(field_name, raw_type, optional)| {
                    (field_name, parse_lua_type(&raw_type, &class_names), optional)
                })
                .collect(),
        })
        .collect();
    classes.sort_by(|a, b| a.name.cmp(&b.name));

    let mut exported_fns = Vec::new();
    let mut frontend_calls = Vec::new();

    for entry in entries {
        let source = match std::str::from_utf8(&entry.data) {
            Ok(s) => s,
            Err(_) => continue,
        };
        let (mut fns, mut calls) = scan_source(source, &entry.name, &class_names)?;
        exported_fns.append(&mut fns);
        frontend_calls.append(&mut calls);
    }

    Ok(LuaScanResult {
        exported_fns,
        frontend_calls,
        classes,
    })
}

fn collect_raw_classes(source: &str, out: &mut HashMap<String, Vec<(String, String, bool)>>) {
    let mut current: Option<(String, Vec<(String, String, bool)>)> = None;

    for line in source.lines() {
        let trimmed = line.trim();
        let content = match trimmed.strip_prefix("--") {
            Some(rest) => rest.trim_start_matches('-').trim(),
            None => {
                if let Some((name, fields)) = current.take() {
                    out.insert(name, fields);
                }
                continue;
            }
        };

        if let Some(rest) = content.strip_prefix("@class ") {
            if let Some((name, fields)) = current.take() {
                out.insert(name, fields);
            }
            let name = rest
                .split(|c: char| c.is_whitespace() || c == ':')
                .next()
                .unwrap_or("")
                .to_string();
            if !name.is_empty() {
                current = Some((name, Vec::new()));
            }
        } else if let Some(rest) = content.strip_prefix("@field ") {
            if let Some((_, fields)) = current.as_mut() {
                let mut parts = rest.splitn(3, ' ');
                let raw_name = parts.next().unwrap_or("_");
                let optional = raw_name.ends_with('?');
                let field_name = raw_name.trim_end_matches('?').to_string();
                let raw_type = parts.next().unwrap_or("unknown").to_string();
                fields.push((field_name, raw_type, optional));
            }
        } else if content.is_empty() {
        } else if let Some((name, fields)) = current.take() {
            out.insert(name, fields);
        }
    }

    if let Some((name, fields)) = current.take() {
        out.insert(name, fields);
    }
}

fn scan_source(
    source: &str,
    file: &str,
    class_names: &HashSet<String>,
) -> anyhow::Result<(Vec<LuaExportedFn>, Vec<LuaFrontendCall>)> {
    let ast = full_moon::parse(source)
        .map_err(|e| anyhow::anyhow!("Lua parse error in {}: {:?}", file, e))?;

    let mut exported_fns = Vec::new();

    for stmt in ast.nodes().stmts() {
        use full_moon::ast::Stmt;
        if let Stmt::FunctionDeclaration(fd) = stmt {
            if let Some(export) = extract_ffi_function(fd, file, class_names) {
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
    class_names: &HashSet<String>,
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
                    params.push((name, parse_lua_type(type_str, class_names)));
                } else if let Some(rest) = content.strip_prefix("@return ") {
                    let type_str = rest.split_whitespace().next().unwrap_or("unknown");
                    return_type = Some(parse_lua_type(type_str, class_names));
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

#[cfg(test)]
mod tests {
    use super::*;

    const FIXTURE: &str = r#"
---@class RpcLibraryResult
---@field ok boolean
---@field error string|nil
---@field games EpicGame[]|nil
---@field refreshed_at integer|nil Unix seconds of the last read from Epic

---@ffi
---@param refresh boolean
---@param force boolean
---@param installed boolean
---@return RpcLibraryResult
function GetLibrary(refresh, force, installed)
    return { ok = true }
end

---@class EpicGame
---@field id string
---@field name string
"#;

    #[test]
    fn collects_classes_with_forward_reference() {
        let mut raw = HashMap::new();
        collect_raw_classes(FIXTURE, &mut raw);

        assert!(raw.contains_key("RpcLibraryResult"));
        assert!(raw.contains_key("EpicGame"));

        let fields = &raw["RpcLibraryResult"];
        assert_eq!(fields.len(), 4);
        assert_eq!(fields[2], ("games".to_string(), "EpicGame[]|nil".to_string(), false));
    }

    #[test]
    fn scan_resolves_class_return_type() {
        let entries = vec![SubEntry {
            name: "main.lua".to_string(),
            data: FIXTURE.as_bytes().to_vec(),
        }];

        let result = scan(&entries).expect("scan should succeed");

        let get_library = result
            .exported_fns
            .iter()
            .find(|f| f.name == "GetLibrary")
            .expect("GetLibrary should be exported");

        assert_eq!(
            get_library.return_type,
            Some(FfiType::Named("RpcLibraryResult".to_string()))
        );

        let rpc_result = result
            .classes
            .iter()
            .find(|c| c.name == "RpcLibraryResult")
            .expect("RpcLibraryResult should be collected");

        let games_field = &rpc_result.fields[2];
        assert_eq!(games_field.0, "games");
        assert_eq!(
            games_field.1,
            FfiType::Union(vec![
                FfiType::Array(Box::new(FfiType::Named("EpicGame".to_string()))),
                FfiType::Nil,
            ])
        );
    }
}
