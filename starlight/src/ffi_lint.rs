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
use crate::ffi_types::FfiType;
use crate::format::section::SubEntry;
use std::path::Path;

fn load_webkit_allowlist() -> anyhow::Result<Option<Vec<String>>> {
    let types_dir = std::env::var("STARLIGHT_TYPES_DIR")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|_| std::path::PathBuf::from("."));
    let json_path = types_dir.join("webkit-exports.json");
    match std::fs::read_to_string(&json_path) {
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => Ok(None),
        Err(e) => Err(anyhow::anyhow!("webkit-exports.json: {}", e)),
        Ok(data) => {
            let list = serde_json::from_str::<Vec<String>>(&data)
                .map_err(|e| anyhow::anyhow!("webkit-exports.json: invalid JSON: {}", e))?;
            Ok(Some(list))
        }
    }
}

/// Returns `(exposed_frontend_fns, lua_ffi_fn_names)`.
pub fn check(
    lua_entries: &[SubEntry],
    config_dir: &Path,
    frontend_entry: Option<&str>,
    webkit_entry: Option<&str>,
) -> anyhow::Result<(Vec<crate::ts_ffi::TsExposedFn>, Vec<String>)> {
    let lua = crate::lua_ffi::scan(lua_entries)?;
    let ts = crate::ts_ffi::scan(config_dir, frontend_entry)?;
    let webkit = crate::ts_ffi::scan(config_dir, webkit_entry)?;
    let frontend_backend_calls = crate::ts_ffi::scan_backend_calls(config_dir, frontend_entry)?;
    let webkit_backend_calls = crate::ts_ffi::scan_backend_calls(config_dir, webkit_entry)?;

    let mut errors = 0usize;

    for call in &ts.ffi_calls {
        let lua_fn = lua.exported_fns.iter().find(|f| f.name == call.fn_name);

        match lua_fn {
            None => {
                crate::log::build_error(
                    &format!("FFI: `{}` is not exported from the backend", call.fn_name),
                    &format!(
                        "  → called in {}{}\n  → add `---@ffi` above `function {}(...)` in Lua\n",
                        call.file,
                        if call.line > 0 {
                            format!(":{}", call.line)
                        } else {
                            String::new()
                        },
                        call.fn_name,
                    ),
                );
                errors += 1;
            }
            Some(lua_fn) => {
                // arity check for ts tuple length vs lua @param count
                if !call.param_types.is_empty() && call.param_types.len() != lua_fn.params.len() {
                    crate::log::build_error(
                        &format!("FFI: arity mismatch for `{}`", call.fn_name),
                        &format!(
                            "  → TypeScript passes {} arg(s), Lua `{}` declares {} param(s)\n",
                            call.param_types.len(),
                            call.fn_name,
                            lua_fn.params.len(),
                        ),
                    );
                    errors += 1;
                }

                // return type compatibility
                if let Some(lua_ret) = &lua_fn.return_type {
                    if !matches!(call.return_type, FfiType::Unknown)
                        && !call.return_type.compatible_with(lua_ret)
                    {
                        crate::log::build_error(
                            &format!("FFI: return type mismatch for `{}`", call.fn_name),
                            &format!(
                                "  → TypeScript expects `{}`, Lua declares `---@return {}`\n",
                                call.return_type, lua_ret,
                            ),
                        );
                        errors += 1;
                    }
                }

                // per-param type compatibility
                for (i, (call_ty, (lua_param_name, lua_ty))) in call
                    .param_types
                    .iter()
                    .zip(lua_fn.params.iter())
                    .enumerate()
                {
                    if !matches!(call_ty, FfiType::Unknown) && !call_ty.compatible_with(lua_ty) {
                        crate::log::build_error(
                            &format!(
                                "FFI: param type mismatch for `{}` arg {} (`{}`)",
                                call.fn_name,
                                i + 1,
                                lua_param_name
                            ),
                            &format!(
                                "  → TypeScript passes `{}`, Lua declares `---@param {} {}`\n",
                                call_ty, lua_param_name, lua_ty,
                            ),
                        );
                        errors += 1;
                    }
                }
            }
        }
    }

    for call in &webkit.ffi_calls {
        if let Some(frontend_name) = call.fn_name.strip_prefix("frontend:") {
            if !ts.exposed_fns.iter().any(|f| f.name == frontend_name) {
                crate::log::build_error(
                    &format!("FFI: webkit calls `frontend:{}` but it is not exported from the frontend", frontend_name),
                    &format!(
                        "  → called in {}{}\n  → add `/** @ffi */ export function {}(...)` in TypeScript\n",
                        call.file,
                        if call.line > 0 { format!(":{}", call.line) } else { String::new() },
                        frontend_name,
                    ),
                );
                errors += 1;
            }
        } else {
            let lua_fn = lua.exported_fns.iter().find(|f| f.name == call.fn_name);
            match lua_fn {
                None => {
                    crate::log::build_error(
                        &format!("FFI: webkit `{}` is not exported from the backend", call.fn_name),
                        &format!(
                            "  → called in {}{}\n  → add `---@ffi` above `function {}(...)` in Lua\n",
                            call.file,
                            if call.line > 0 { format!(":{}", call.line) } else { String::new() },
                            call.fn_name,
                        ),
                    );
                    errors += 1;
                }
                Some(lua_fn) => {
                    if !call.param_types.is_empty() && call.param_types.len() != lua_fn.params.len()
                    {
                        crate::log::build_error(
                            &format!("FFI: webkit arity mismatch for `{}`", call.fn_name),
                            &format!(
                                "  → webkit passes {} arg(s), Lua `{}` declares {} param(s)\n",
                                call.param_types.len(),
                                call.fn_name,
                                lua_fn.params.len(),
                            ),
                        );
                        errors += 1;
                    }
                    if let Some(lua_ret) = &lua_fn.return_type {
                        if !matches!(call.return_type, FfiType::Unknown)
                            && !call.return_type.compatible_with(lua_ret)
                        {
                            crate::log::build_error(
                                &format!("FFI: webkit return type mismatch for `{}`", call.fn_name),
                                &format!(
                                    "  → webkit expects `{}`, Lua declares `---@return {}`\n",
                                    call.return_type, lua_ret,
                                ),
                            );
                            errors += 1;
                        }
                    }
                }
            }
        }
    }

    // webkit import allowlist check, reject sharedjscontext symbols in webkit files
    if webkit_entry.is_some() {
        match load_webkit_allowlist()? {
            None => {
                crate::log::warn(
                    "webkit: webkit-exports.json not found — import symbol checks skipped",
                );
            }
            Some(allowlist) => {
                let webkit_imports = crate::ts_ffi::scan_webkit_imports(config_dir, webkit_entry)?;
                for imp in &webkit_imports {
                    if !allowlist.iter().any(|a| a == &imp.name) {
                        crate::log::build_error(
                            &format!("webkit: `{}` is not available in a webview context", imp.name),
                            &format!(
                                "  -> imported in {}:{}\n  -> `{}` is a frontend-only export from 'millennium'\n  -> only millennium-api exports are available in webview\n",
                                imp.file, imp.line, imp.name,
                            ),
                        );
                        errors += 1;
                    }
                }
            }
        }

        let banned = crate::ts_ffi::scan_banned_package_imports(
            config_dir,
            webkit_entry,
            &["@steambrew/client", "@steambrew/webkit"],
        )?;
        for imp in &banned {
            crate::log::build_error(
                &format!(
                    "webkit: `{}` is deprecated and cannot be used with the starlight compiler",
                    imp.package
                ),
                &format!(
                    "  -> imported in {}:{}\n  -> use the `millennium` package instead\n",
                    imp.file, imp.line,
                ),
            );
            errors += 1;
        }
    }

    if frontend_entry.is_some() {
        let banned = crate::ts_ffi::scan_banned_package_imports(
            config_dir,
            frontend_entry,
            &["@steambrew/client", "@steambrew/webkit"],
        )?;
        for imp in &banned {
            crate::log::build_error(
                &format!(
                    "frontend: `{}` is deprecated and cannot be used with the starlight compiler",
                    imp.package
                ),
                &format!(
                    "  -> imported in {}:{}\n  -> use the `millennium` package instead\n",
                    imp.file, imp.line,
                ),
            );
            errors += 1;
        }
    }

    for call in frontend_backend_calls
        .iter()
        .chain(webkit_backend_calls.iter())
    {
        match lua.exported_fns.iter().find(|f| f.name == call.method) {
            None => {
                crate::log::build_error(
                    &format!(
                        "FFI: `backend.{}` is not exported from the backend",
                        call.method
                    ),
                    &format!(
                        "  → called in {}:{}\n  → add `---@ffi` above `function {}(...)` in Lua\n",
                        call.file, call.line, call.method,
                    ),
                );
                errors += 1;
            }
            Some(lua_fn) if call.arg_count != lua_fn.params.len() => {
                crate::log::build_error(
                    &format!("FFI: arity mismatch for `backend.{}`", call.method),
                    &format!(
                        "  → called with {} arg(s) in {}:{}, but `{}` declares {} param(s)\n",
                        call.arg_count,
                        call.file,
                        call.line,
                        call.method,
                        lua_fn.params.len(),
                    ),
                );
                errors += 1;
            }
            _ => {}
        }
    }

    // call_frontend_method("name") in lua -> must match a /** @ffi */ ts fn
    for lua_call in &lua.frontend_calls {
        if !ts.exposed_fns.iter().any(|f| f.name == lua_call.fn_name) {
            crate::log::warn(&format!(
                "FFI: `{}` called from Lua ({}{}) but no `/** @ffi */` function found in TypeScript",
                lua_call.fn_name,
                lua_call.file,
                if lua_call.line > 0 { format!(":{}", lua_call.line) } else { String::new() },
            ));
        }
    }

    // always regenerate types from the current Lua state so the LSP stays accurate
    // even when validation fails and the build is rejected.
    let lua_fn_names: Vec<String> = lua.exported_fns.iter().map(|f| f.name.clone()).collect();
    if !lua.exported_fns.is_empty() {
        generate_dts(&lua.exported_fns, config_dir)?;
    }

    // generate `declare const frontend` types for webkit bundles when there are exposed TS fns
    if webkit_entry.is_some() && !ts.exposed_fns.is_empty() {
        generate_frontend_dts(&ts.exposed_fns, config_dir)?;
    }

    if errors > 0 {
        return Err(anyhow::anyhow!(
            "FFI validation failed ({} error(s))",
            errors
        ));
    }

    Ok((ts.exposed_fns, lua_fn_names))
}

fn generate_frontend_dts(
    exports: &[crate::ts_ffi::TsExposedFn],
    config_dir: &Path,
) -> anyhow::Result<()> {
    let mut out = String::from(
        "// Auto-generated by starlight. Do not manually edit.\n\ndeclare const frontend: {\n",
    );

    for func in exports {
        let location = if func.line > 0 {
            format!("{}:{}", func.file, func.line)
        } else {
            func.file.clone()
        };

        let params_str = func
            .raw_params
            .iter()
            .map(|(name, ty)| format!("{}: {}", name, ty))
            .collect::<Vec<_>>()
            .join(", ");

        let ret_str = &func.raw_return_type;

        out.push_str(&format!("    /** {} */\n", location));
        out.push_str(&format!(
            "    {}: ({}) => Promise<{}>;\n",
            func.name, params_str, ret_str,
        ));
    }

    out.push_str("};\n");

    let dir = config_dir.join(".millennium/types/frontend");
    std::fs::create_dir_all(&dir)
        .map_err(|e| anyhow::anyhow!("cannot create {}: {}", dir.display(), e))?;
    let path = dir.join("index.d.ts");
    std::fs::write(&path, &out)
        .map_err(|e| anyhow::anyhow!("cannot write {}: {}", path.display(), e))?;

    Ok(())
}

fn generate_dts(
    exports: &[crate::lua_ffi::LuaExportedFn],
    config_dir: &Path,
) -> anyhow::Result<()> {
    let mut out = String::from(
        "// Auto-generated by starlight. Do not manually edit.\n\ndeclare const backend: {\n",
    );

    for func in exports {
        let location = if func.line > 0 {
            format!("{}:{}", func.file, func.line)
        } else {
            func.file.clone()
        };

        let params_str = func
            .params
            .iter()
            .map(|(name, ty)| format!("{}: {}", name, ty.to_ts()))
            .collect::<Vec<_>>()
            .join(", ");

        let ret_str = func
            .return_type
            .as_ref()
            .map(|t| t.to_ts())
            .unwrap_or_else(|| "unknown".to_string());

        out.push_str(&format!("    /** {} */\n", location));
        out.push_str(&format!(
            "    {}: ({}) => Promise<{}>;\n",
            func.name, params_str, ret_str,
        ));
    }

    out.push_str("};\n");

    let dir = config_dir.join(".millennium/types/backend");
    std::fs::create_dir_all(&dir)
        .map_err(|e| anyhow::anyhow!("cannot create {}: {}", dir.display(), e))?;
    let path = dir.join("index.d.ts");
    std::fs::write(&path, &out)
        .map_err(|e| anyhow::anyhow!("cannot write {}: {}", path.display(), e))?;

    Ok(())
}
