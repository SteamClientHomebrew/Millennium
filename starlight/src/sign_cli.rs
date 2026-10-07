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
use std::path::Path;

use crate::format::header::{SectionEntry, HEADER_SIZE, SECTION_ENTRY_SIZE, STAR_FLAG_SIGNED};
use crate::format::section::{META_FLAG_DEFERRED, SECTION_ASSETS};

const SIG_LEN: usize = 64;

pub fn sign(path: &Path) -> anyhow::Result<()> {
    let mut data = std::fs::read(path)
        .map_err(|e| anyhow::anyhow!("cannot read {}: {}", path.display(), e))?;

    let (header, entries, plg_start) = crate::verify::parse_header(&data)?;
    anyhow::ensure!(
        header.star_flags & STAR_FLAG_SIGNED == 0,
        "{} is already signed",
        path.display()
    );

    let table_start = plg_start + HEADER_SIZE;

    if let Some((i, entry)) = entries
        .iter()
        .enumerate()
        .find(|(_, e)| e.id == SECTION_ASSETS)
    {
        let patched = SectionEntry {
            id: entry.id,
            encode_flags: entry.encode_flags,
            meta_flags: entry.meta_flags,
            offset: entry.offset + SIG_LEN as u64,
            length: entry.length,
            crc32: entry.crc32,
        };
        let entry_start = table_start + i * SECTION_ENTRY_SIZE;
        data[entry_start..entry_start + SECTION_ENTRY_SIZE].copy_from_slice(&patched.to_bytes());
    }

    let max_eager_end: usize = entries
        .iter()
        .filter(|e| e.meta_flags & META_FLAG_DEFERRED == 0)
        .map(|e| e.offset as usize + e.length as usize)
        .max()
        .unwrap_or(0);
    let sig_start = plg_start + max_eager_end;

    let flags_offset = plg_start + 7;
    data[flags_offset] |= STAR_FLAG_SIGNED;

    let signature = crate::signing::sign_bytes(&data[..sig_start])?;

    let mut out = Vec::with_capacity(data.len() + SIG_LEN);
    out.extend_from_slice(&data[..sig_start]);
    out.extend_from_slice(&signature);
    out.extend_from_slice(&data[sig_start..]);

    std::fs::write(path, &out)
        .map_err(|e| anyhow::anyhow!("cannot write {}: {}", path.display(), e))?;

    crate::log::tag("Sign", crate::log::Color::BabyBlue)
        .info(&format!("Signed {}", path.display()));
    Ok(())
}
