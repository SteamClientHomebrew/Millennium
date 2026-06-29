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
const SHIM_JS: &str = include_str!("shim.js");
const INSPECT_POLYFILL_JS: &str = include_str!("inspect_polyfill.js");

/// Returns `(wrapped_js, iife_line_offset, iife_col_offset)`.
/// Offsets describe where `__BUNDLE__` lands in the final output so the
/// rolldown source map can be shifted to point at the right lines.
pub fn wrap(
    bundle_js: &str,
    plugin_name: &str,
    is_client: bool,
    source_map_url: Option<&str>,
    inspect: &crate::config::InspectConfig,
    lua_ffi_names: &[String],
) -> (String, u32, u32) {
    let iife_expr = extract_iife(strip_sourcemap(bundle_js).trim_end());

    let plugin_name_json = serde_json::to_string(plugin_name).expect("plugin name is valid JSON");
    let source_tag = if is_client { "frontend" } else { "webkit" };

    let ffi_stubs = if lua_ffi_names.is_empty() {
        String::new()
    } else {
        let methods: String = lua_ffi_names
            .iter()
            .map(|n| format!("  {}: window.MILLENNIUM_API.ffi(pluginName, '{}'),\n", n, n))
            .collect();
        format!("const backend = {{\n{}}};\n", methods)
    };

    let frontend_proxy = if !is_client {
        format!(
            "const frontend = new Proxy({{}}, {{\n  \
             get(_, key) {{\n    \
             if (typeof key !== 'string') return undefined;\n    \
             return window.MILLENNIUM_API.ffi({}, 'frontend:' + key);\n  \
             }}\n}});\n",
            plugin_name_json
        )
    } else {
        String::new()
    };

    let post_message = if is_client {
        "if (MILLENNIUM_IS_CLIENT_MODULE) {\n    \
         MILLENNIUM_BACKEND_IPC.postMessage(1, { pluginName: pluginName });\n  \
         }"
    } else {
        ""
    };

    // Render the shim with all substitutions except __BUNDLE__.
    // We need the pre-bundle text to count the line/col offset.
    let shim = SHIM_JS
        .replace("__IS_CLIENT__", if is_client { "true" } else { "false" })
        .replace("__PLUGIN_NAME__", &plugin_name_json)
        .replace("__FFI_STUBS__;", &ffi_stubs)
        .replace("__FRONTEND_PROXY__;", &frontend_proxy)
        .replace("__INSPECT_POLYFILL__;", INSPECT_POLYFILL_JS)
        .replace("__POST_MESSAGE__;", post_message)
        .replace("__SOURCE_TAG__", source_tag)
        .replace("__DEPTH__", &inspect.depth.to_string())
        .replace("__COLORS__", &inspect.colors.to_string())
        .replace("__SHOW_HIDDEN__", &inspect.show_hidden.to_string());

    let bundle_placeholder = "__BUNDLE__";
    let bundle_pos = shim
        .find(bundle_placeholder)
        .expect("shim.js missing __BUNDLE__");
    let before = &shim[..bundle_pos];

    let iife_line_offset = before.bytes().filter(|&b| b == b'\n').count() as u32;
    let iife_col_offset = (before.len() - before.rfind('\n').map_or(0, |p| p + 1)) as u32;

    let after = &shim[bundle_pos + bundle_placeholder.len()..];
    let sourcemap_comment = match source_map_url {
        Some(url) => format!("\n//# sourceMappingURL={url}"),
        None => String::new(),
    };

    let full = format!("{before}{iife_expr}{after}{sourcemap_comment}\n");
    (full, iife_line_offset, iife_col_offset)
}

fn extract_iife(code: &str) -> &str {
    let code = code.strip_suffix(';').unwrap_or(code);
    if let Some(rest) = code.strip_prefix("var ") {
        rest.splitn(2, " = ").nth(1).unwrap_or(code)
    } else {
        code
    }
}

fn strip_sourcemap(code: &str) -> &str {
    if let Some(pos) = code.rfind("//# sourceMappingURL=") {
        return code[..pos].trim_end_matches('\n').trim_end_matches('\r');
    }
    code
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config::InspectConfig;

    fn default_inspect() -> InspectConfig {
        InspectConfig {
            depth: 2,
            colors: false,
            show_hidden: false,
        }
    }

    #[test]
    fn sourcemap_url_present() {
        let bundle = "(function() { return {}; })()";
        let (out, _, _) = wrap(
            bundle,
            "test",
            true,
            Some("file:///test.map"),
            &default_inspect(),
            &[],
        );
        assert!(
            out.contains("//# sourceMappingURL=file:///test.map"),
            "missing sourceMappingURL\nlast 300: {:?}",
            &out[out.len().saturating_sub(300)..]
        );
    }

    #[test]
    fn sourcemap_url_absent_when_none() {
        let bundle = "(function() { return {}; })()";
        let (out, _, _) = wrap(bundle, "test", true, None, &default_inspect(), &[]);
        assert!(
            !out.contains("sourceMappingURL"),
            "unexpected sourceMappingURL"
        );
    }
}
