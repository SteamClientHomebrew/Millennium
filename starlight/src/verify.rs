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

use crate::format::{
    header::{
        fnv1a, SectionEntry, StarHeader, HEADER_SIZE, MAGIC, SECTION_ENTRY_SIZE, STAR_FLAG_SIGNED,
    },
    obfuscate::xor_deobfuscate,
    parity::verify_and_strip_parity,
    section::{
        parse_sub_entries, section_name, PluginMetadata, FLAG_COMPRESSED, FLAG_OBFUSCATED,
        META_FLAG_DEFERRED, SECTION_ASSETS, SECTION_BACKEND, SECTION_FRONTEND, SECTION_METADATA,
        SECTION_WEBKIT,
    },
};

pub fn verify(path: &Path) -> anyhow::Result<()> {
    let data = std::fs::read(path)
        .map_err(|e| anyhow::anyhow!("cannot read {}: {}", path.display(), e))?;

    let (header, entries, plg_start) = parse_header(&data)?;
    let signed_label = if header.star_flags & crate::format::header::STAR_FLAG_SIGNED != 0 {
        "signed"
    } else {
        "unsigned"
    };
    println!(
        "starlight v{}.{} — {} section(s)  [shim {} bytes, {}]",
        header.version_major, header.version_minor, header.section_count, plg_start, signed_label,
    );

    for entry in &entries {
        let blob = read_section_blob(&data, plg_start, entry)?;
        let crc = crc32fast::hash(blob);
        anyhow::ensure!(
            crc == entry.crc32,
            "CRC32 mismatch in section {} '{}': stored {:08x}, computed {:08x}",
            entry.id,
            section_name(entry.id),
            entry.crc32,
            crc
        );
        let decoded = decode_section(blob, entry.encode_flags)?;
        print_section(entry, &decoded)?;
    }

    let sig_label = if header.star_flags & crate::format::header::STAR_FLAG_SIGNED != 0 {
        "signature OK"
    } else {
        "unsigned"
    };
    println!("\nOK — all sections verified ({}).", sig_label);
    Ok(())
}

pub fn inspect(path: &Path) -> anyhow::Result<()> {
    let data = std::fs::read(path)
        .map_err(|e| anyhow::anyhow!("cannot read {}: {}", path.display(), e))?;

    let (header, entries, plg_start) = parse_header(&data)?;
    println!(
        "starlight v{}.{} — {} section(s)  [shim {} bytes]",
        header.version_major, header.version_minor, header.section_count, plg_start,
    );

    for entry in &entries {
        let blob = match read_section_blob(&data, plg_start, entry) {
            Ok(b) => b,
            Err(e) => {
                eprintln!("  [section {:02x}] read error: {}", entry.id, e);
                continue;
            }
        };
        let decoded = match decode_section_best_effort(blob, entry.encode_flags) {
            Ok(d) => d,
            Err(e) => {
                eprintln!("  [section {:02x}] decode error: {}", entry.id, e);
                continue;
            }
        };
        if let Err(e) = print_section(entry, &decoded) {
            eprintln!("  [section {:02x}] parse error: {}", entry.id, e);
        }
    }
    Ok(())
}

pub fn query(path: &Path, q: &str) -> anyhow::Result<()> {
    let data = std::fs::read(path)
        .map_err(|e| anyhow::anyhow!("cannot read {}: {}", path.display(), e))?;

    let (_header, entries, plg_start) = parse_header(&data)?;

    if q == "version" {
        let entry = entries
            .iter()
            .find(|e| e.id == SECTION_METADATA)
            .ok_or_else(|| anyhow::anyhow!("no metadata section in {}", path.display()))?;
        let blob = read_section_blob(&data, plg_start, entry)?;
        let decoded = decode_section_best_effort(blob, entry.encode_flags)?;
        let meta: PluginMetadata = rmp_serde::from_slice(&decoded)
            .map_err(|e| anyhow::anyhow!("metadata parse error: {}", e))?;
        println!("{}", meta.starlight_version);
        return Ok(());
    }

    let (target_id, file_filter): (u8, Option<&str>) = if q == "metadata" {
        (SECTION_METADATA, None)
    } else if q == "frontend" {
        (SECTION_FRONTEND, None)
    } else if let Some(rest) = q.strip_prefix("frontend:") {
        (SECTION_FRONTEND, Some(rest))
    } else if q == "backend" {
        (SECTION_BACKEND, None)
    } else if let Some(rest) = q.strip_prefix("backend:") {
        (SECTION_BACKEND, Some(rest))
    } else if q == "webkit" {
        (SECTION_WEBKIT, None)
    } else if let Some(rest) = q.strip_prefix("webkit:") {
        (SECTION_WEBKIT, Some(rest))
    } else {
        anyhow::bail!(
            "unknown query '{}'; valid: version, metadata, frontend[:<file>], backend[:<file>], webkit[:<file>]",
            q
        );
    };

    let entry = entries
        .iter()
        .find(|e| e.id == target_id)
        .ok_or_else(|| anyhow::anyhow!("section '{}' not found in {}", q, path.display()))?;

    let blob = read_section_blob(&data, plg_start, entry)?;
    let decoded = decode_section_best_effort(blob, entry.encode_flags)?;

    match target_id {
        SECTION_METADATA => {
            let meta: PluginMetadata = rmp_serde::from_slice(&decoded)
                .map_err(|e| anyhow::anyhow!("metadata parse error: {}", e))?;
            println!("{}", serde_json::to_string_pretty(&meta)?);
        }
        SECTION_FRONTEND | SECTION_BACKEND | SECTION_WEBKIT => {
            let sub = parse_sub_entries(&decoded)?;
            if let Some(name) = file_filter {
                let f = sub.iter().find(|e| e.name == name).ok_or_else(|| {
                    anyhow::anyhow!(
                        "file '{}' not found in {} section",
                        name,
                        section_name(target_id)
                    )
                })?;
                use std::io::Write as _;
                std::io::stdout().write_all(&f.data)?;
            } else {
                let list: Vec<serde_json::Value> = sub
                    .iter()
                    .map(|e| serde_json::json!({ "name": e.name, "size": e.data.len() }))
                    .collect();
                println!("{}", serde_json::to_string_pretty(&list)?);
            }
        }
        _ => unreachable!(),
    }

    Ok(())
}

fn parse_header(data: &[u8]) -> anyhow::Result<(StarHeader, Vec<SectionEntry>, usize)> {
    anyhow::ensure!(data.len() >= 4, "file too short");

    let plg_start = if &data[0..4] == MAGIC.as_slice() {
        0 // legacy: PLGHeader at offset 0, no shim prefix
    } else {
        // Shimmed: [shim_len u32][raw_shim][PLGHeader]
        let shim_len = u32::from_le_bytes(data[0..4].try_into().unwrap()) as usize;
        anyhow::ensure!(
            data.len() >= 4 + shim_len,
            "shim prefix extends beyond file"
        );
        4 + shim_len
    };

    let plg = &data[plg_start..];
    anyhow::ensure!(
        plg.len() >= HEADER_SIZE,
        "file too short to contain a header"
    );
    let header = StarHeader::from_bytes(plg[0..HEADER_SIZE].try_into().unwrap())?;

    let table_end = HEADER_SIZE + header.section_count as usize * SECTION_ENTRY_SIZE;
    anyhow::ensure!(
        plg.len() >= table_end,
        "file too short to contain section table"
    );

    let entries =
        SectionEntry::read_table(&plg[HEADER_SIZE..table_end], header.section_count as usize)?;

    // The signature (if present) covers everything up to max_eager_end, not the end of the file.
    // Assets and other deferred sections live after the signature.
    if header.star_flags & STAR_FLAG_SIGNED != 0 {
        const SIG_LEN: usize = 64;

        let max_eager_end: usize = entries
            .iter()
            .filter(|e| e.meta_flags & META_FLAG_DEFERRED == 0)
            .map(|e| e.offset as usize + e.length as usize)
            .max()
            .unwrap_or(0);

        let sig_start = plg_start + max_eager_end;
        anyhow::ensure!(
            data.len() >= sig_start + SIG_LEN,
            "STAR_FLAG_SIGNED is set but file is too short to contain a signature"
        );

        let payload = &data[..sig_start];
        let sig_bytes: &[u8; SIG_LEN] = data[sig_start..sig_start + SIG_LEN].try_into().unwrap();
        let vk_bytes = crate::signing::get_verifying_key_bytes()?;

        anyhow::ensure!(
            crate::signing::verify_star(payload, sig_bytes, &vk_bytes),
            "Ed25519 signature verification failed — file has been tampered with or signed with a different key"
        );
    }

    // verify shim integrity when a shim prefix is present.
    if plg_start > 0 {
        let shim_bytes = &data[4..plg_start]; // skip shim_len(4)
        let computed = fnv1a(shim_bytes);
        anyhow::ensure!(
            computed == header.shim_integrity,
            "shim integrity check failed: stored {:08x}, computed {:08x} — shim may have been tampered with",
            header.shim_integrity,
            computed
        );
    }

    Ok((header, entries, plg_start))
}

fn read_section_blob<'a>(
    data: &'a [u8],
    plg_start: usize,
    entry: &SectionEntry,
) -> anyhow::Result<&'a [u8]> {
    let start = plg_start + entry.offset as usize;
    let end = start + entry.length as usize;
    anyhow::ensure!(
        end <= data.len(),
        "section {:02x} extends beyond file ({} > {})",
        entry.id,
        end,
        data.len()
    );
    Ok(&data[start..end])
}

fn decode_section(blob: &[u8], encode_flags: u8) -> anyhow::Result<Vec<u8>> {
    if encode_flags == 0 {
        return Ok(blob.to_vec());
    }
    let mut data = blob.to_vec();
    if encode_flags & FLAG_OBFUSCATED != 0 {
        xor_deobfuscate(&mut data);
    }
    let stripped = verify_and_strip_parity(&data)?;
    if encode_flags & FLAG_COMPRESSED != 0 {
        return lz4_flex::block::decompress_size_prepended(&stripped)
            .map_err(|e| anyhow::anyhow!("lz4 decompress failed: {}", e));
    }
    Ok(stripped)
}

fn decode_section_best_effort(blob: &[u8], encode_flags: u8) -> anyhow::Result<Vec<u8>> {
    if encode_flags == 0 {
        return Ok(blob.to_vec());
    }
    let mut data = blob.to_vec();
    if encode_flags & FLAG_OBFUSCATED != 0 {
        xor_deobfuscate(&mut data);
    }
    let stripped = match verify_and_strip_parity(&data) {
        Ok(s) => s,
        Err(_) => data,
    };
    if encode_flags & FLAG_COMPRESSED != 0 {
        return lz4_flex::block::decompress_size_prepended(&stripped)
            .map_err(|e| anyhow::anyhow!("lz4 decompress failed: {}", e));
    }
    Ok(stripped)
}

fn print_section(entry: &SectionEntry, decoded: &[u8]) -> anyhow::Result<()> {
    println!(
        "\n  [{:02x}] {} ({} bytes on disk, encode={:02x} meta={:08x})",
        entry.id,
        section_name(entry.id),
        entry.length,
        entry.encode_flags,
        entry.meta_flags,
    );
    if entry.id == SECTION_METADATA {
        let meta: PluginMetadata = rmp_serde::from_slice(decoded)
            .map_err(|e| anyhow::anyhow!("metadata msgpack parse error: {}", e))?;
        println!("      id:               {}", meta.id);
        println!("      name:             {}", meta.name);
        println!("      version:          {}", meta.version);
        println!("      author:           {}", meta.author);
        if !meta.description.is_empty() {
            println!("      description:      {}", meta.description);
        }
        println!("      starlight_version:  {}", meta.starlight_version);
    } else if entry.id == SECTION_BACKEND
        || entry.id == SECTION_FRONTEND
        || entry.id == SECTION_WEBKIT
    {
        let entries = parse_sub_entries(decoded)?;
        println!("      {} file(s):", entries.len());
        for e in &entries {
            println!("        {} ({} bytes)", e.name, e.data.len());
        }
    } else if entry.id == SECTION_ASSETS {
        let sub = parse_sub_entries(decoded)?;
        println!("      {} asset(s):", sub.len());
        for e in &sub {
            let uncompressed = lz4_flex::block::decompress_size_prepended(&e.data)
                .map(|v| v.len())
                .unwrap_or(0);
            println!("        {} ({} B compressed → {} B)", e.name, e.data.len(), uncompressed);
        }
    }
    Ok(())
}
