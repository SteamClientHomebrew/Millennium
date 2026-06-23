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
use crate::BuildMode;
use indexmap::IndexMap;
use rolldown::{
    AddonOutputOption, Bundler, BundlerOptions, GlobalsOutputOption, InputItem, IsExternal,
    OutputExports, OutputFormat, RawMinifyOptions, SourceMapType,
};
use rustc_hash::FxHasher;
use std::hash::BuildHasherDefault;
type FxIndexMap<K, V> = IndexMap<K, V, BuildHasherDefault<FxHasher>>;

#[derive(Debug)]
pub struct BundleCompileError;
impl std::fmt::Display for BundleCompileError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "bundle compile error")
    }
}
impl std::error::Error for BundleCompileError {}
use rustc_hash::FxHashMap;
use std::collections::HashMap;
use std::path::Path;

use super::transforms::{
    apply_transforms, CONSOLE_HOOK, FRONTEND_INJECT_ARGS, FRONTEND_INJECT_CONSTS, FRONTEND_RENAMES,
    WEBKIT_INJECT_ARGS, WEBKIT_INJECT_CONSTS, WEBKIT_RENAMES,
};
use super::wrapper::wrap;

static FRONTEND_DEFAULTS: &[(&str, &str)] = &[
    ("@steambrew/client", "window.MILLENNIUM_API"),
    ("react", "window.SP_REACT"),
    ("react-dom", "window.SP_REACTDOM"),
    ("react-dom/client", "window.SP_REACTDOM"),
    ("react/jsx-runtime", "SP_JSX_FACTORY"),
];

static WEBKIT_DEFAULTS: &[(&str, &str)] = &[("@steambrew/webkit", "window.MILLENNIUM_API")];

pub enum BundleTarget {
    Frontend,
    Webkit,
}

pub fn bundle(
    entry: &str,
    config_dir: &Path,
    user_globals: &HashMap<String, String>,
    target: BundleTarget,
    plugin_name: &str,
    mode: BuildMode,
    source_map_url: Option<&str>,
    inspect: &crate::config::InspectConfig,
    outro: Option<String>,
) -> anyhow::Result<(Vec<u8>, Vec<u8>)> {
    let rt = tokio::runtime::Runtime::new()
        .map_err(|e| anyhow::anyhow!("failed to create tokio runtime: {}", e))?;
    rt.block_on(async_bundle(
        entry,
        config_dir,
        user_globals,
        target,
        plugin_name,
        mode,
        source_map_url,
        inspect,
        outro,
    ))
}

async fn async_bundle(
    entry: &str,
    config_dir: &Path,
    user_globals: &HashMap<String, String>,
    target: BundleTarget,
    plugin_name: &str,
    mode: BuildMode,
    source_map_url: Option<&str>,
    inspect: &crate::config::InspectConfig,
    outro: Option<String>,
) -> anyhow::Result<(Vec<u8>, Vec<u8>)> {
    let entry_path = if Path::new(entry).is_absolute() {
        entry.to_string()
    } else {
        config_dir
            .join(entry)
            .to_str()
            .ok_or_else(|| anyhow::anyhow!("non-UTF-8 entry path"))?
            .to_string()
    };

    let defaults = match target {
        BundleTarget::Frontend => FRONTEND_DEFAULTS,
        BundleTarget::Webkit => WEBKIT_DEFAULTS,
    };
    let mut merged: FxHashMap<String, String> = defaults
        .iter()
        .map(|(k, v)| (k.to_string(), v.to_string()))
        .collect();
    for (k, v) in user_globals {
        merged.insert(k.clone(), v.clone());
    }

    let external_names: Vec<String> = merged.keys().cloned().collect();

    let define: FxIndexMap<String, String> = [("import.meta".to_string(), "{}".to_string())]
        .into_iter()
        .collect();

    let (exports, outro_opt) = match target {
        BundleTarget::Webkit => (Some(OutputExports::Named), None),
        BundleTarget::Frontend => (
            // Named so @ffi exports + default coexist without MIXED_EXPORTS warning
            Some(OutputExports::Named),
            outro.map(|s| AddonOutputOption::String(Some(s))),
        ),
    };

    let mut bundler = Bundler::new(BundlerOptions {
        input: Some(vec![InputItem {
            name: Some("bundle".into()),
            import: entry_path,
        }]),
        cwd: Some(config_dir.to_path_buf()),
        format: Some(OutputFormat::Iife),
        name: Some("plugin".into()),
        external: Some(IsExternal::from(external_names)),
        globals: Some(GlobalsOutputOption::FxHashMap(merged)),
        minify: Some(RawMinifyOptions::Bool(false)),
        sourcemap: if mode == BuildMode::Debug {
            Some(SourceMapType::File)
        } else {
            None
        },
        define: Some(define),
        exports,
        outro: outro_opt,
        ..Default::default()
    })
    .map_err(|e| anyhow::anyhow!("rolldown init error: {:#?}", e))?;

    let output = match bundler.generate().await {
        Ok(out) => out,
        Err(batch) => {
            let body: String = batch
                .iter()
                .map(|d| d.to_diagnostic().to_color_string())
                .collect::<Vec<_>>()
                .join("");
            crate::log::build_error("Bundle error", &body);
            return Err(BundleCompileError.into());
        }
    };

    if !output.warnings.is_empty() {
        let body: String = output
            .warnings
            .iter()
            .map(|w| w.to_diagnostic().to_color_string())
            .collect::<Vec<_>>()
            .join("");
        crate::log::build_error("Bundle warnings", &body);
    }

    let mut raw_js = String::new();
    let mut map_bytes: Vec<u8> = Vec::new();
    for asset in &output.assets {
        if asset.filename().ends_with(".js.map") {
            map_bytes.extend_from_slice(asset.content_as_bytes());
        } else if asset.filename().ends_with(".js") {
            raw_js = String::from_utf8_lossy(asset.content_as_bytes()).into_owned();
        }
    }
    anyhow::ensure!(!raw_js.is_empty(), "rolldown produced no .js output");

    let (ia, rn, ic) = match target {
        BundleTarget::Frontend => (
            FRONTEND_INJECT_ARGS,
            FRONTEND_RENAMES,
            FRONTEND_INJECT_CONSTS,
        ),
        BundleTarget::Webkit => (WEBKIT_INJECT_ARGS, WEBKIT_RENAMES, WEBKIT_INJECT_CONSTS),
    };
    let sm = if !map_bytes.is_empty() {
        sourcemap::SourceMap::from_slice(&map_bytes).ok()
    } else {
        None
    };
    let transformed = apply_transforms(&raw_js, ia, rn, ic, Some(&CONSOLE_HOOK), sm.as_ref());

    let is_client = matches!(target, BundleTarget::Frontend);
    let wrapped = wrap(
        &transformed,
        plugin_name,
        is_client,
        source_map_url,
        inspect,
    );

    let final_js = if mode == BuildMode::Release {
        crate::minify::minify_js(&wrapped)
    } else {
        wrapped
    };

    Ok((final_js.into_bytes(), map_bytes))
}
