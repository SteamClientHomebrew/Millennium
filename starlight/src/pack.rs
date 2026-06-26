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
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use crate::format::{
    header::{SectionEntry, StarHeader, HEADER_SIZE, SECTION_ENTRY_SIZE, STAR_FLAG_SIGNED},
    obfuscate::xor_obfuscate,
    parity::weave_parity,
    section::{
        serialize_sub_entries, PluginMetadata, SubEntry, FLAG_COMPRESSED, FLAG_OBFUSCATED,
        META_FLAG_DEFERRED, SECTION_ASSETS, SECTION_BACKEND, SECTION_FRONTEND, SECTION_METADATA,
        SECTION_WEBKIT,
    },
};
use crate::{
    bundler::{
        js::{self, BundleTarget},
        lua,
    },
    BuildMode,
};

const SHIM_RAW: &[u8] = include_bytes!("shim.lua");

pub fn pack(
    config_path: &Path,
    out_path: Option<&Path>,
    mode: BuildMode,
) -> anyhow::Result<Option<crate::config::DevRuntime>> {
    let start = std::time::Instant::now();
    let config_path = config_path
        .canonicalize()
        .map_err(|_| anyhow::anyhow!("config not found: {}", config_path.display()))?;
    let config_dir = config_path.parent().unwrap();
    let cfg = crate::config::load(&config_path)?;

    if let Err(e) = crate::lsp::install_types(config_dir, env!("CARGO_PKG_VERSION"), &cfg) {
        crate::log::warn(&format!("lsp: failed to install types: {}", e));
    }

    let out_path = match out_path {
        Some(p) => p.to_path_buf().join(format!("{}.star", cfg.plugin.id)),
        None => match cfg.compiler.as_ref().and_then(|c| c.output_path.as_deref()) {
            Some("auto") => resolve_auto_output(&cfg.plugin.id)?,
            Some(p) => PathBuf::from(p),
            None => config_dir.join(format!("{}.star", cfg.plugin.id)),
        },
    };

    let metadata = PluginMetadata {
        id: cfg.plugin.id.clone(),
        name: cfg.plugin.name.clone(),
        version: cfg.plugin.version.clone(),
        author: cfg.plugin.author.clone(),
        description: cfg.plugin.description.clone().unwrap_or_default(),
        starlight_version: env!("CARGO_PKG_VERSION").to_string(),
        entry: cfg
            .backend
            .as_ref()
            .map(|b| b.entry.clone())
            .unwrap_or_default(),
    };
    let metadata_blob = rmp_serde::to_vec_named(&metadata)
        .map_err(|e| anyhow::anyhow!("msgpack serialization failed: {}", e))?;

    // collect assets before building section table (need to know count)
    let asset_sub_entries = collect_assets(config_dir, &cfg.assets.resources)?;
    let has_assets = !asset_sub_entries.is_empty();

    let mut raw_sections: Vec<(u8, Vec<u8>)> = vec![(SECTION_METADATA, metadata_blob)];

    let mut ffi_exposed: Vec<crate::ts_ffi::TsExposedFn> = Vec::new();
    let mut lua_ffi_names: Vec<String> = Vec::new();

    if let Some(backend_cfg) = &cfg.backend {
        let mut entries = lua::collect(&backend_cfg.sources, config_dir)?;
        let lint = crate::ffi_lint::check(
            &entries,
            config_dir,
            cfg.frontend.as_ref().map(|f| f.entry.as_str()),
            cfg.webkit.as_ref().map(|w| w.entry.as_str()),
        )?;
        ffi_exposed = lint.0;
        lua_ffi_names = lint.1;
        if mode == BuildMode::Release {
            for entry in &mut entries {
                entry.data = crate::minify::minify_lua(&entry.data);
            }
        }
        raw_sections.push((SECTION_BACKEND, serialize_sub_entries(&entries)));
    }

    if let Some(frontend_cfg) = &cfg.frontend {
        let (map_disk_path, map_url) = if mode == BuildMode::Debug {
            let sm_dir = prepare_sourcemap_dir(config_dir, "bundle.js.")?;
            let name = format!("bundle.js.{}.map", now_secs());
            let p = sm_dir.join(&name);
            let url = format!("file://{}", p.display());
            (Some(p), Some(url))
        } else {
            (None, None)
        };

        let ffi_names: Vec<&str> = ffi_exposed.iter().map(|f| f.name.as_str()).collect();
        let outro = if ffi_names.is_empty() {
            None
        } else {
            Some(format!(
                "Millennium.exposeObj({{ {} }});",
                ffi_names.join(", ")
            ))
        };

        let (js_bytes, map_bytes) = js::bundle(
            &frontend_cfg.entry,
            config_dir,
            &frontend_cfg.globals,
            BundleTarget::Frontend,
            &cfg.plugin.id,
            mode,
            map_url.as_deref(),
            &cfg.inspect,
            outro,
            &lua_ffi_names,
        )?;

        if !ffi_exposed.is_empty() {
            let js_text = std::str::from_utf8(&js_bytes).unwrap_or("");
            let defined = collect_named_functions(js_text);
            let mut bundle_errors = 0usize;
            for f in &ffi_exposed {
                if !defined.contains(f.name.as_str()) {
                    crate::log::build_error(
                        &format!("FFI: `{}` is annotated with `/** @ffi */` but is not reachable from the frontend entry point", f.name),
                        &format!(
                            "  → declared in {}:{}\n  → re-export it from the entry file: `export {{ {} }} from '...'`\n",
                            f.file, f.line, f.name,
                        ),
                    );
                    bundle_errors += 1;
                }
            }
            if bundle_errors > 0 {
                return Err(anyhow::anyhow!(
                    "FFI validation failed ({} error(s))",
                    bundle_errors
                ));
            }
        }

        if let (Some(path), false) = (&map_disk_path, map_bytes.is_empty()) {
            std::fs::write(path, &map_bytes).map_err(|e| {
                anyhow::anyhow!("cannot write source map {}: {}", path.display(), e)
            })?;
        }

        raw_sections.push((
            SECTION_FRONTEND,
            serialize_sub_entries(&[SubEntry {
                name: "bundle.js".into(),
                data: js_bytes,
            }]),
        ));
    }

    if let Some(webkit_cfg) = &cfg.webkit {
        let (map_disk_path, map_url) = if mode == BuildMode::Debug {
            let sm_dir = prepare_sourcemap_dir(config_dir, "webkit_bundle.js.")?;
            let name = format!("webkit_bundle.js.{}.map", now_secs());
            let p = sm_dir.join(&name);
            let url = format!("file://{}", p.display());
            (Some(p), Some(url))
        } else {
            (None, None)
        };

        let (js_bytes, map_bytes) = js::bundle(
            &webkit_cfg.entry,
            config_dir,
            &webkit_cfg.globals,
            BundleTarget::Webkit,
            &cfg.plugin.id,
            mode,
            map_url.as_deref(),
            &cfg.inspect,
            None,
            &lua_ffi_names,
        )?;

        if let (Some(path), false) = (&map_disk_path, map_bytes.is_empty()) {
            std::fs::write(path, &map_bytes).map_err(|e| {
                anyhow::anyhow!("cannot write source map {}: {}", path.display(), e)
            })?;
        }

        raw_sections.push((
            SECTION_WEBKIT,
            serialize_sub_entries(&[SubEntry {
                name: "bundle.js".into(),
                data: js_bytes,
            }]),
        ));
    }

    let encoded: Vec<(u8, u8, Vec<u8>)> = raw_sections
        .iter()
        .map(|(id, blob)| {
            let (bytes, encode_flags) = encode_section(blob);
            (*id, encode_flags, bytes)
        })
        .collect();

    // assets section is included in section count and table but NOT in encoded vec
    let section_count = encoded.len() + if has_assets { 1 } else { 0 };
    let header_region = HEADER_SIZE + section_count * SECTION_ENTRY_SIZE;

    // build non-asset section entries with sequential offsets
    let mut offset: u64 = header_region as u64;
    let mut section_entries: Vec<SectionEntry> = Vec::new();
    for (id, encode_flags, bytes) in &encoded {
        section_entries.push(SectionEntry {
            id: *id,
            encode_flags: *encode_flags,
            meta_flags: 0,
            offset,
            length: bytes.len() as u64,
            crc32: crc32fast::hash(bytes),
        });
        offset += bytes.len() as u64;
    }
    // `offset` is now the end of all non-asset section data (relative to star_start)

    // serialize asset sub-entries now so we know the size for the section entry
    let asset_blob: Vec<u8> = if has_assets {
        serialize_sub_entries(&asset_sub_entries)
    } else {
        Vec::new()
    };

    let shim = crate::minify::minify_lua(SHIM_RAW);
    let shim_len = shim.len() as u32;

    // asset data is placed after the signature; compute how large the sig gap will be
    let will_sign = std::env::var("STARLIGHT_SIGNING_KEY").is_ok();
    let sig_gap: u64 = if will_sign { 64 } else { 0 };

    if has_assets {
        section_entries.push(SectionEntry {
            id: SECTION_ASSETS,
            encode_flags: 0,
            meta_flags: META_FLAG_DEFERRED,
            // offset is relative to star_start; asset data begins after non-asset data + sig
            offset: offset + sig_gap,
            length: asset_blob.len() as u64,
            crc32: crc32fast::hash(&asset_blob),
        });
    }

    let mut out: Vec<u8> = Vec::new();
    out.extend_from_slice(&shim_len.to_le_bytes());
    out.extend_from_slice(&shim);
    out.extend_from_slice(&StarHeader::new(section_count as u8, &shim).to_bytes());
    for entry in &section_entries {
        out.extend_from_slice(&entry.to_bytes());
    }
    for (_, _, bytes) in &encoded {
        out.extend_from_slice(bytes);
    }

    // sign covers shim + header + section_table + non-asset section data (not asset blob)
    let signed = match crate::signing::try_sign_star(&out)? {
        Some(sig) => {
            out.extend_from_slice(&sig);
            true
        }
        None => {
            let flags_offset = 4 + shim.len() + 7;
            out[flags_offset] &= !STAR_FLAG_SIGNED;
            false
        }
    };

    // asset data goes lastl; outside the signed region and never read by bounded parsers
    if has_assets {
        out.extend_from_slice(&asset_blob);
    }

    std::fs::write(&out_path, &out)
        .map_err(|e| anyhow::anyhow!("cannot write {}: {}", out_path.display(), e))?;

    let elapsed = start.elapsed();
    let mode_str = if mode == BuildMode::Debug {
        "debug"
    } else {
        "release"
    };
    let sign_str = if signed {
        crate::log::green("signed")
    } else {
        crate::log::orange("not signed")
    };
    let out_filename = out_path
        .file_stem()
        .and_then(|n| n.to_str())
        .unwrap_or("output");
    let out_display = out_path.display().to_string();
    let finished = format!(
        "{} {} {} in {:.2}s {}{}{}",
        crate::log::green("Compiled"),
        out_filename,
        mode_str,
        elapsed.as_secs_f64(),
        crate::log::dim(&format!("({}, ", fmt_size(out.len()))),
        sign_str,
        crate::log::dim(&format!(" v{}) → {}", cfg.plugin.version, out_display)),
    );
    crate::log::tag("starlight", crate::log::Color::Rgb(255, 200, 50)).info(&finished);

    let dev = cfg.dev.as_ref().map(|dev| crate::config::DevRuntime {
        plugin_name: dev
            .plugin_name
            .as_deref()
            .unwrap_or(&cfg.plugin.id)
            .to_owned(),
        socket: dev
            .socket
            .clone()
            .unwrap_or_else(crate::mep::default_socket),
        auto_restart: dev.auto_restart,
        reload_steamui_when: dev.reload_steamui_when.clone(),
    });

    Ok(dev)
}

fn fmt_size(bytes: usize) -> String {
    const GB: f64 = 1024.0 * 1024.0 * 1024.0;
    const MB: f64 = 1024.0 * 1024.0;
    const KB: f64 = 1024.0;
    let b = bytes as f64;
    if b >= GB {
        format!("{:.2}GB", b / GB)
    } else if b >= MB {
        format!("{:.2}MB", b / MB)
    } else if b >= KB {
        format!("{:.2}kB", b / KB)
    } else {
        format!("{}B", bytes)
    }
}

fn now_secs() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

fn prepare_sourcemap_dir(out_dir: &Path, prefix: &str) -> anyhow::Result<PathBuf> {
    let dir = out_dir.join(".millennium/sourcemaps");
    std::fs::create_dir_all(&dir)
        .map_err(|e| anyhow::anyhow!("cannot create {}: {}", dir.display(), e))?;
    if let Ok(entries) = std::fs::read_dir(&dir) {
        for entry in entries.flatten() {
            let name = entry.file_name();
            let name = name.to_string_lossy();
            if name.starts_with(prefix) && name.ends_with(".map") {
                let _ = std::fs::remove_file(entry.path());
            }
        }
    }
    Ok(dir)
}

fn resolve_auto_output(plugin_id: &str) -> anyhow::Result<PathBuf> {
    let filename = format!("{}.star", plugin_id);

    #[cfg(target_os = "linux")]
    {
        let data_home = std::env::var("XDG_DATA_HOME")
            .map(PathBuf::from)
            .unwrap_or_else(|_| {
                let home = std::env::var("HOME")
                    .map(PathBuf::from)
                    .expect("auto: neither $XDG_DATA_HOME nor $HOME is set");
                home.join(".local/share")
            });
        let path = data_home.join("millennium/plugins").join(&filename);
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent).map_err(|e| {
                anyhow::anyhow!(
                    "auto: cannot create plugins dir {}: {}",
                    parent.display(),
                    e
                )
            })?;
        }
        return Ok(path);
    }

    #[cfg(target_os = "windows")]
    {
        use winreg::enums::*;
        use winreg::RegKey;

        let steam_path = RegKey::predef(HKEY_LOCAL_MACHINE)
            .open_subkey("SOFTWARE\\Valve\\Steam")
            .or_else(|_| {
                RegKey::predef(HKEY_LOCAL_MACHINE)
                    .open_subkey("SOFTWARE\\WOW6432Node\\Valve\\Steam")
            })
            .or_else(|_| RegKey::predef(HKEY_CURRENT_USER).open_subkey("SOFTWARE\\Valve\\Steam"))
            .and_then(|key| key.get_value::<String, _>("InstallPath"))
            .map_err(|_| anyhow::anyhow!("auto: Steam installation not found in registry"))?;

        let path = PathBuf::from(steam_path)
            .join("millennium/plugins")
            .join(&filename);
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent).map_err(|e| {
                anyhow::anyhow!(
                    "auto: cannot create plugins dir {}: {}",
                    parent.display(),
                    e
                )
            })?;
        }
        return Ok(path);
    }

    #[cfg(not(any(target_os = "linux", target_os = "windows")))]
    anyhow::bail!("auto output_path is not supported on this platform")
}

fn encode_section(blob: &[u8]) -> (Vec<u8>, u8) {
    let compressed = lz4_flex::block::compress_prepend_size(blob);
    let mut woven = weave_parity(&compressed);
    xor_obfuscate(&mut woven);
    (woven, FLAG_OBFUSCATED | FLAG_COMPRESSED)
}

/// Collect asset files from `resources` patterns into sub-entries with per-file lz4 compression.
/// Returns an empty vec if there are no resources configured.
fn collect_assets(config_dir: &Path, resources: &[String]) -> anyhow::Result<Vec<SubEntry>> {
    if resources.is_empty() {
        return Ok(Vec::new());
    }

    let canonical_root = config_dir
        .canonicalize()
        .map_err(|e| anyhow::anyhow!("assets: cannot canonicalize plugin dir: {}", e))?;

    let mut map: indexmap::IndexMap<String, Vec<u8>> = indexmap::IndexMap::new();

    for pattern in resources {
        let is_glob = pattern.contains('*') || pattern.contains('?') || pattern.contains('[');

        if is_glob {
            let full_pattern = config_dir.join(pattern).to_string_lossy().into_owned();
            let matches: Vec<_> = glob::glob(&full_pattern)
                .map_err(|e| anyhow::anyhow!("assets: invalid glob '{}': {}", pattern, e))?
                .filter_map(|r| r.ok())
                .collect();

            if matches.is_empty() {
                crate::log::warn(&format!("assets: glob '{}' matched no files", pattern));
            }

            for path in matches {
                if path
                    .symlink_metadata()
                    .map(|m| m.file_type().is_symlink())
                    .unwrap_or(false)
                {
                    continue;
                }
                if path.is_dir() {
                    collect_dir_assets(&path, &canonical_root, &mut map)?;
                } else if path.is_file() {
                    add_asset_file(&path, &canonical_root, &mut map)?;
                }
            }
        } else {
            let path = config_dir.join(pattern);
            if !path.exists() {
                anyhow::bail!("assets: '{}' not found", pattern);
            }
            if path.is_dir() {
                collect_dir_assets(&path, &canonical_root, &mut map)?;
            } else {
                add_asset_file(&path, &canonical_root, &mut map)?;
            }
        }
    }

    Ok(map
        .into_iter()
        .map(|(name, raw)| SubEntry {
            name,
            data: lz4_flex::block::compress_prepend_size(&raw),
        })
        .collect())
}

fn collect_dir_assets(
    dir: &Path,
    root: &Path,
    map: &mut indexmap::IndexMap<String, Vec<u8>>,
) -> anyhow::Result<()> {
    for entry in std::fs::read_dir(dir)
        .map_err(|e| anyhow::anyhow!("assets: cannot read dir {}: {}", dir.display(), e))?
    {
        let entry = entry.map_err(|e| anyhow::anyhow!("assets: dir entry error: {}", e))?;
        let path = entry.path();
        let meta = entry
            .metadata()
            .map_err(|e| anyhow::anyhow!("assets: cannot stat {}: {}", path.display(), e))?;
        if meta.file_type().is_symlink() {
            continue;
        }
        if meta.is_dir() {
            collect_dir_assets(&path, root, map)?;
        } else if meta.is_file() {
            add_asset_file(&path, root, map)?;
        }
    }
    Ok(())
}

fn add_asset_file(
    path: &Path,
    root: &Path,
    map: &mut indexmap::IndexMap<String, Vec<u8>>,
) -> anyhow::Result<()> {
    if path
        .symlink_metadata()
        .map(|m| m.file_type().is_symlink())
        .unwrap_or(false)
    {
        crate::log::warn(&format!("assets: skipping symlink '{}'", path.display()));
        return Ok(());
    }

    let canonical = path
        .canonicalize()
        .map_err(|e| anyhow::anyhow!("assets: cannot canonicalize {}: {}", path.display(), e))?;

    if !canonical.starts_with(root) {
        crate::log::warn(&format!(
            "assets: skipping '{}' (resolves outside plugin directory)",
            path.display()
        ));
        return Ok(());
    }

    let rel = canonical.strip_prefix(root).unwrap();
    let name = rel
        .components()
        .map(|c| c.as_os_str().to_string_lossy().into_owned())
        .collect::<Vec<_>>()
        .join("/");

    let data = std::fs::read(&canonical)
        .map_err(|e| anyhow::anyhow!("assets: cannot read {}: {}", canonical.display(), e))?;

    map.insert(name, data);
    Ok(())
}

fn collect_named_functions(js: &str) -> std::collections::HashSet<String> {
    use oxc_allocator::Allocator;
    use oxc_ast::ast::{Expression, Function, Statement};
    use oxc_parser::Parser;
    use oxc_span::SourceType;

    fn visit_fn(func: &Function<'_>, out: &mut std::collections::HashSet<String>) {
        if let Some(id) = &func.id {
            out.insert(id.name.to_string());
        }
        if let Some(body) = &func.body {
            for stmt in &body.statements {
                visit_stmt(stmt, out);
            }
        }
    }

    fn visit_expr(expr: &Expression<'_>, out: &mut std::collections::HashSet<String>) {
        match expr {
            Expression::FunctionExpression(f) => visit_fn(f, out),
            Expression::ArrowFunctionExpression(f) => {
                for stmt in &f.body.statements {
                    visit_stmt(stmt, out);
                }
            }
            Expression::CallExpression(call) => {
                visit_expr(&call.callee, out);
                for arg in &call.arguments {
                    if let Some(e) = arg.as_expression() {
                        visit_expr(e, out);
                    }
                }
            }
            Expression::AssignmentExpression(a) => {
                if let oxc_ast::ast::AssignmentTarget::StaticMemberExpression(m) = &a.left {
                    out.insert(m.property.name.to_string());
                }
                visit_expr(&a.right, out);
            }
            Expression::SequenceExpression(seq) => {
                for e in &seq.expressions {
                    visit_expr(e, out);
                }
            }
            Expression::ParenthesizedExpression(p) => visit_expr(&p.expression, out),
            _ => {}
        }
    }

    fn visit_stmt(stmt: &Statement<'_>, out: &mut std::collections::HashSet<String>) {
        match stmt {
            Statement::FunctionDeclaration(f) => visit_fn(f, out),
            Statement::ExpressionStatement(es) => visit_expr(&es.expression, out),
            Statement::BlockStatement(b) => {
                for s in &b.body {
                    visit_stmt(s, out);
                }
            }
            Statement::VariableDeclaration(vd) => {
                for decl in &vd.declarations {
                    if let Some(init) = &decl.init {
                        visit_expr(init, out);
                    }
                }
            }
            Statement::ReturnStatement(r) => {
                if let Some(arg) = &r.argument {
                    visit_expr(arg, out);
                }
            }
            _ => {}
        }
    }

    let alloc = Allocator::default();
    let ret = Parser::new(&alloc, js, SourceType::mjs()).parse();
    let mut names = std::collections::HashSet::new();
    for stmt in &ret.program.body {
        visit_stmt(stmt, &mut names);
    }
    names
}
