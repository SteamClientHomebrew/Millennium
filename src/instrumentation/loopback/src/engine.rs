use crate::ipc::{self, PatchDef};
use regex::{Regex, RegexSet};
use std::path::Path;

const TEXT_CAP: usize = 500;

pub struct CompiledPatch {
    pub plugin: String,
    pub find: Regex,
    pub transforms: Vec<(Regex, String)>,
}

pub struct PatchSet {
    pub file_set: RegexSet,
    pub patches: Vec<CompiledPatch>,
}

impl PatchSet {
    pub fn build(defs: Vec<PatchDef>) -> Option<Self> {
        let file_patterns: Vec<&str> = defs.iter().map(|d| d.file.as_str()).collect();
        let file_set = match RegexSet::new(&file_patterns) {
            Ok(s) => s,
            Err(e) => {
                log::error!(
                    "PatchSet::build: failed to compile {} file pattern(s), discarding entire patch list: {}",
                    file_patterns.len(),
                    e
                );
                return None;
            }
        };

        let mut patches = Vec::with_capacity(defs.len());
        for def in defs {
            let find = match Regex::new(&def.find) {
                Ok(r) => r,
                Err(e) => {
                    log::warn!("'{}': invalid find regex {:?}: {}", def.plugin, def.find, e);
                    continue;
                }
            };
            let mut transforms = Vec::new();
            for (m, r) in def.transforms {
                match Regex::new(&m) {
                    Ok(pat) => {
                        let r = expand_self(&r, &def.plugin);
                        transforms.push((pat, re2_to_rust_rep(&r)));
                    }
                    Err(e) => {
                        log::warn!("'{}': invalid transform regex {:?}: {}", def.plugin, m, e);
                    }
                }
            }
            patches.push(CompiledPatch {
                plugin: def.plugin,
                find,
                transforms,
            });
        }

        Some(PatchSet { file_set, patches })
    }
}

fn expand_self(s: &str, plugin_name: &str) -> String {
    s.replace(
        "#{{self}}",
        &format!("window.PLUGIN_LIST['{}']", plugin_name),
    )
}

fn re2_to_rust_rep(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut chars = s.chars().peekable();
    while let Some(c) = chars.next() {
        if c == '\\' {
            if let Some(&d) = chars.peek() {
                if d.is_ascii_digit() && d != '0' {
                    out.push('$');
                    out.push(chars.next().unwrap());
                    continue;
                }
            }
        }
        out.push(c);
    }
    out
}

fn cap_text(s: &str) -> String {
    let bytes = s.as_bytes();
    let mut out: Vec<u8> = bytes[..bytes.len().min(TEXT_CAP)]
        .iter()
        .map(|&b| if b < 32 || b == 127 { b' ' } else { b })
        .collect();
    if bytes.len() > TEXT_CAP {
        out.extend_from_slice(b"...");
    }
    String::from_utf8_lossy(&out).into_owned()
}

fn short_name(filename: &str) -> &str {
    Path::new(filename)
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or(filename)
}

fn apply_scoped(buf: &mut Vec<u8>, from: usize, to: usize, pat: &Regex, rep: &str) -> usize {
    let region = match std::str::from_utf8(&buf[from..to]) {
        Ok(s) => s.to_owned(),
        Err(_) => return to,
    };
    let replaced = pat.replace_all(&region, rep);
    let new_bytes = replaced.as_bytes();
    let new_to = from + new_bytes.len();
    buf.splice(from..to, new_bytes.iter().copied());
    new_to
}

pub fn apply_patches(
    patch_set: &PatchSet,
    content: &[u8],
    filename: &str,
) -> Option<(Vec<u8>, Vec<String>)> {
    let content_str = std::str::from_utf8(content).ok()?;

    let matched_indices: Vec<usize> = patch_set.file_set.matches(filename).into_iter().collect();

    if matched_indices.is_empty() {
        return None;
    }

    let name = short_name(filename);
    let kb = content.len() as f64 / 1024.0;

    ipc::send_patch_event(
        "",
        "",
        filename,
        "url_seen",
        &format!("{:.1} KB", kb),
        "",
        "",
        "",
        "",
    );

    let mut matched_patches: Vec<&CompiledPatch> = matched_indices
        .iter()
        .map(|&i| &patch_set.patches[i])
        .collect();

    matched_patches.sort_by(|a, b| a.plugin.cmp(&b.plugin));

    let plugin_names: Vec<&str> = matched_patches.iter().map(|p| p.plugin.as_str()).collect();
    log::info!(
        "{}: {} plugin(s) matched ({:.1} KB): [{}]",
        name,
        matched_patches.len(),
        kb,
        plugin_names.join(", ")
    );

    detect_conflicts(&matched_patches, content_str, filename);

    let mut buf: Vec<u8> = content.to_vec();
    let mut contributing: Vec<String> = Vec::new();

    for patch in &matched_patches {
        let current = match std::str::from_utf8(&buf) {
            Ok(s) => s.to_owned(),
            Err(_) => continue,
        };

        let m = match patch.find.find(&current) {
            Some(m) => m,
            None => {
                log::warn!(
                    "{}: '{}': find pattern had no match in current buffer (patch skipped)",
                    name,
                    patch.plugin
                );
                ipc::send_patch_event(
                    &patch.plugin,
                    "",
                    filename,
                    "no_match",
                    "",
                    patch.find.as_str(),
                    "",
                    "",
                    "",
                );
                continue;
            }
        };

        let (from, mut to) = (m.start(), m.end());
        let scope_size_before = to - from;
        let before_text = cap_text(&current[from..to]);

        let transform_patterns: String = patch
            .transforms
            .iter()
            .map(|(p, r)| format!("/{p}/ -> {r}"))
            .collect::<Vec<_>>()
            .join("\n");

        for (pat, rep) in &patch.transforms {
            to = apply_scoped(&mut buf, from, to, pat, rep);
        }

        let scope_size_after = to - from;
        let size_delta = scope_size_after as i64 - scope_size_before as i64;
        let after_str = std::str::from_utf8(&buf[from..to]).unwrap_or("").to_owned();
        let after_text = cap_text(&after_str);

        log::info!(
            "{}: '{}': matched [{}, {}), {} transform(s), {:+} B",
            name,
            patch.plugin,
            from,
            to,
            patch.transforms.len(),
            size_delta,
        );

        let detail = format!("[{from}, {to}), {} transform(s)", patch.transforms.len());
        ipc::send_patch_event(
            &patch.plugin,
            "",
            filename,
            "match",
            &detail,
            patch.find.as_str(),
            &transform_patterns,
            &before_text,
            &after_text,
        );

        contributing.push(patch.plugin.clone());
    }

    if contributing.is_empty() {
        None
    } else {
        Some((buf, contributing))
    }
}

fn detect_conflicts(patches: &[&CompiledPatch], content: &str, filename: &str) {
    let regions: Vec<Option<(usize, usize)>> = patches
        .iter()
        .map(|p| p.find.find(content).map(|m| (m.start(), m.end())))
        .collect();

    let name = short_name(filename);

    for i in 0..patches.len() {
        for j in (i + 1)..patches.len() {
            if patches[i].plugin == patches[j].plugin {
                continue;
            }
            if let (Some((a, b)), Some((c, d))) = (regions[i], regions[j]) {
                if a < d && c < b {
                    let (winner, loser) = if patches[i].plugin < patches[j].plugin {
                        (&patches[i].plugin, &patches[j].plugin)
                    } else {
                        (&patches[j].plugin, &patches[i].plugin)
                    };
                    log::warn!(
                        "{} CONFLICT: '{}' and '{}' overlap [{},{}) ∩ [{},{}) - '{}' wins, '{}' will be skipped",
                        name, winner, loser, a, b, c, d, winner, loser
                    );
                    let detail = format!("[{a}, {b}) vs [{c}, {d})");
                    ipc::send_patch_event(
                        winner, loser, filename, "conflict", &detail, "", "", "", "",
                    );
                }
            }
        }
    }
}
