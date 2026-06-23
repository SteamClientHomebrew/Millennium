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
        SECTION_BACKEND, SECTION_FRONTEND, SECTION_METADATA, SECTION_WEBKIT,
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

    let out_path = match out_path {
        Some(p) => p.to_path_buf(),
        None => config_dir.join("plugin.star"),
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

    let mut raw_sections: Vec<(u8, Vec<u8>)> = vec![(SECTION_METADATA, metadata_blob)];

    let mut ffi_exposed: Vec<String> = Vec::new();

    if let Some(backend_cfg) = &cfg.backend {
        let mut entries = lua::collect(&backend_cfg.sources, config_dir)?;
        ffi_exposed = crate::ffi_lint::check(
            &entries,
            config_dir,
            cfg.frontend.as_ref().map(|f| f.entry.as_str()),
            cfg.webkit.as_ref().map(|w| w.entry.as_str()),
        )?;
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

        let outro = if ffi_exposed.is_empty() {
            None
        } else {
            Some(format!(
                "Millennium.exposeObj({{ {} }});",
                ffi_exposed.join(", ")
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
        )?;

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
            let (bytes, flags) = encode_section(blob);
            (*id, flags, bytes)
        })
        .collect();

    let section_count = encoded.len() as u8;
    let header_region = HEADER_SIZE + section_count as usize * SECTION_ENTRY_SIZE;

    let mut offset = header_region as u32;
    let mut section_entries: Vec<SectionEntry> = Vec::new();
    for (id, flags, bytes) in &encoded {
        section_entries.push(SectionEntry {
            id: *id,
            flags: *flags,
            offset,
            length: bytes.len() as u32,
            crc32: crc32fast::hash(bytes),
        });
        offset += bytes.len() as u32;
    }

    let shim = crate::minify::minify_lua(SHIM_RAW);

    let shim_len = shim.len() as u32;
    let mut out: Vec<u8> = Vec::new();
    out.extend_from_slice(&shim_len.to_le_bytes());
    out.extend_from_slice(&shim);
    out.extend_from_slice(&StarHeader::new(section_count, &shim).to_bytes());
    for entry in &section_entries {
        out.extend_from_slice(&entry.to_bytes());
    }
    for (_, _, bytes) in &encoded {
        out.extend_from_slice(bytes);
    }

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

    std::fs::write(&out_path, &out)
        .map_err(|e| anyhow::anyhow!("cannot write {}: {}", out_path.display(), e))?;

    let elapsed = start.elapsed();
    let mode_str = if mode == BuildMode::Debug {
        "debug [unoptimized + debuginfo]"
    } else {
        "release"
    };
    let sign_str = if signed {
        crate::log::green("signed")
    } else {
        crate::log::orange("not signed")
    };
    crate::log::info(&format!(
        "{} {} in {:.2}s {}{}{}",
        crate::log::green("Finished"),
        crate::log::underline(mode_str),
        elapsed.as_secs_f64(),
        crate::log::dim(&format!(
            "({:.2}kB, {} sections, ",
            out.len() as f64 / 1024.0,
            section_count
        )),
        sign_str,
        crate::log::dim(&format!(" v{})", cfg.plugin.version)),
    ));

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

fn encode_section(blob: &[u8]) -> (Vec<u8>, u8) {
    let compressed = lz4_flex::block::compress_prepend_size(blob);
    let mut woven = weave_parity(&compressed);
    xor_obfuscate(&mut woven);
    (woven, FLAG_OBFUSCATED | FLAG_COMPRESSED)
}
