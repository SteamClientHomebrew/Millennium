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
use crate::format::section::SubEntry;
use std::path::Path;

pub fn collect(sources: &[String], config_dir: &Path) -> anyhow::Result<Vec<SubEntry>> {
    let mut entries: Vec<SubEntry> = Vec::new();
    let mut seen = std::collections::HashSet::new();

    for pattern in sources {
        let full_pattern = if Path::new(pattern).is_absolute() {
            pattern.clone()
        } else {
            config_dir
                .join(pattern)
                .to_str()
                .ok_or_else(|| anyhow::anyhow!("non-UTF-8 path in sources glob"))?
                .to_string()
        };

        let paths = glob::glob(&full_pattern)
            .map_err(|e| anyhow::anyhow!("invalid glob pattern '{}': {}", pattern, e))?;

        for result in paths {
            let abs = result.map_err(|e| anyhow::anyhow!("glob error: {}", e))?;
            if !abs.is_file() {
                continue;
            }
            let canonical = abs
                .canonicalize()
                .map_err(|e| anyhow::anyhow!("cannot canonicalize {}: {}", abs.display(), e))?;
            if !seen.insert(canonical.clone()) {
                continue;
            }

            let rel = canonical
                .strip_prefix(config_dir)
                .unwrap_or(&canonical)
                .to_str()
                .ok_or_else(|| anyhow::anyhow!("non-UTF-8 path: {}", canonical.display()))?
                .replace('\\', "/");

            let data = std::fs::read(&canonical)
                .map_err(|e| anyhow::anyhow!("cannot read {}: {}", canonical.display(), e))?;

            entries.push(SubEntry { name: rel, data });
        }
    }

    anyhow::ensure!(
        !entries.is_empty(),
        "no Lua source files matched the backend sources globs"
    );
    Ok(entries)
}
