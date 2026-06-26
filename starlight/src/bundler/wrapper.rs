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
const INSPECT_POLYFILL_JS: &str = include_str!("inspect_polyfill.js");

pub fn wrap(
    bundle_js: &str,
    plugin_name: &str,
    is_client: bool,
    source_map_url: Option<&str>,
    inspect: &crate::config::InspectConfig,
    lua_ffi_names: &[String],
) -> String {
    let code = strip_sourcemap(bundle_js).trim_end();

    let code = code.strip_suffix(';').unwrap_or(code);

    let iife_expr = if let Some(rest) = code.strip_prefix("var ") {
        rest.splitn(2, " = ").nth(1).unwrap_or(code)
    } else {
        code
    };

    let plugin_name_json = serde_json::to_string(plugin_name).expect("plugin name is valid JSON");
    let is_client_str = if is_client { "true" } else { "false" };

    let post_message = if is_client {
        "\t\tif (MILLENNIUM_IS_CLIENT_MODULE) {\n\
         \t\t\tMILLENNIUM_BACKEND_IPC.postMessage(1, { pluginName: pluginName });\n\
         \t\t}\n"
    } else {
        ""
    };

    let source_tag = if is_client { "frontend" } else { "webkit" };
    let console_helper = format!(
        "var __millennium_plugin_console__ = (function() {{\n\
         {polyfill}\
         \n\
         return function(level, file, line) {{\n\
         \tvar args = Array.prototype.slice.call(arguments, 3);\n\
         \ttry {{\n\
         \t\tvar MAX_MSG = 65536;\n\
         \t\tvar msg = args.map(function(a) {{\n\
         \t\t\ttry {{\n\
         \t\t\t\tif (typeof a === 'string') return a;\n\
         \t\t\t\tif (a === null) return 'null';\n\
         \t\t\t\tif (a === undefined) return 'undefined';\n\
         \t\t\t\tif (typeof a === 'number' || typeof a === 'boolean') return String(a);\n\
         \t\t\t\treturn __inspect(a, {{depth: {depth}, colors: {colors}, showHidden: {show_hidden}}});\n\
         \t\t\t}} catch(e) {{ return '[' + (e && e.message ? e.message : String(e)) + ']'; }}\n\
         \t\t}}).join(' ');\n\
         \t\tif (msg.length > MAX_MSG) msg = msg.slice(0, MAX_MSG) + ' [truncated]';\n\
         \t\tvar payload = {{ plugin: {pname}, level: level, message: msg, source: \"{src}\" }};\n\
         \t\tif (file !== undefined) {{ payload.file = file; payload.line = line; }}\n\
         \t\twindow.__millennium_plugin_console_binding__(JSON.stringify(payload));\n\
         \t}} catch(e) {{\n\
         \t\ttry {{\n\
         \t\t\tvar errPayload = {{ plugin: {pname}, level: 'error', message: '[console] ' + (e && e.message ? e.message : String(e)), source: \"{src}\" }};\n\
         \t\t\twindow.__millennium_plugin_console_binding__(JSON.stringify(errPayload));\n\
         \t\t}} catch(_) {{}}\n\
         \t}}\n\
         }};\n\
         }})();\n",
        polyfill = INSPECT_POLYFILL_JS,
        pname = plugin_name_json,
        src = source_tag,
        depth = inspect.depth,
        colors = inspect.colors,
        show_hidden = inspect.show_hidden,
    );

    let ffi_stubs = if lua_ffi_names.is_empty() {
        String::new()
    } else {
        let methods = lua_ffi_names
            .iter()
            .map(|name| {
                format!(
                    "    {}: window.MILLENNIUM_API.ffi(pluginName, '{}'),\n",
                    name, name
                )
            })
            .collect::<String>();
        format!("const backend = {{\n{}}};\n", methods)
    };

    let frontend_proxy = if !is_client {
        format!(
            "const frontend = new Proxy({{}}, {{\n\
             \tget(_, key) {{\n\
             \t\tif (typeof key !== 'string') return undefined;\n\
             \t\treturn window.MILLENNIUM_API.ffi({}, 'frontend:' + key);\n\
             \t}}\n\
             }});\n",
            plugin_name_json
        )
    } else {
        String::new()
    };

    let sourcemap_comment = match source_map_url {
        Some(url) => format!("\n//# sourceMappingURL={url}"),
        None => String::new(),
    };

    format!(
        "const MILLENNIUM_IS_CLIENT_MODULE = {is_client};\n\
         const pluginName = {plugin_name_json};\n\
         (window.PLUGIN_LIST ||= {{}})[pluginName] ||= {{}};\n\
         window.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS ||= {{}};\n\
         {ffi_stubs}\
         {frontend_proxy}\
         \n\
         {console_helper}\
         \n\
         let PluginEntryPointMain = function () {{\n\
         \tconst __exports = {{}};\n\
         \tconst __bundle = {iife};\n\
         \tif (__bundle && typeof __bundle === 'object' && 'default' in __bundle) {{\n\
         \t\tObject.assign(__exports, __bundle);\n\
         \t}} else {{\n\
         \t\t__exports.default = __bundle;\n\
         \t}}\n\
         \treturn __exports;\n\
         }};\n\
         \n\
         (async () => {{\n\
         \tconst PluginModule = PluginEntryPointMain();\n\
         \tObject.assign(window.PLUGIN_LIST[pluginName], {{\n\
         \t\t...PluginModule,\n\
         \t\t__millennium_internal_plugin_name_do_not_use_or_change__: pluginName,\n\
         \t}});\n\
         \tconst pluginProps = await PluginModule.default();\n\
         \tif (\n\
         \t\tpluginProps &&\n\
         \t\tpluginProps.title !== undefined &&\n\
         \t\tpluginProps.icon !== undefined &&\n\
         \t\tpluginProps.content !== undefined\n\
         \t) {{\n\
         \t\twindow.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS[pluginName] = pluginProps;\n\
         \t}}\n\
         {post_message}\
         }})();{sourcemap_comment}\n",
        is_client = is_client_str,
        plugin_name_json = plugin_name_json,
        ffi_stubs = ffi_stubs,
        frontend_proxy = frontend_proxy,
        iife = iife_expr,
        post_message = post_message,
        sourcemap_comment = sourcemap_comment,
    )
}

fn strip_sourcemap(code: &str) -> &str {
    // Strip trailing `//# sourceMappingURL=...` line.
    if let Some(pos) = code.rfind("//# sourceMappingURL=") {
        let before = code[..pos].trim_end_matches('\n').trim_end_matches('\r');
        return before;
    }
    code
}
