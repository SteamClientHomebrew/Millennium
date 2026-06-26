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
use serde::{Deserialize, Serialize};

pub const SECTION_METADATA: u8 = 0x01;
pub const SECTION_BACKEND: u8 = 0x02;
pub const SECTION_FRONTEND: u8 = 0x03;
pub const SECTION_WEBKIT: u8 = 0x04;
pub const SECTION_ASSETS: u8 = 0x05;

pub const FLAG_OBFUSCATED: u8 = 0x40;
pub const FLAG_COMPRESSED: u8 = 0x80;
#[allow(dead_code)]
pub const FLAG_SOURCE_MAP: u8 = 0x20;

pub const META_FLAG_DEFERRED: u32 = 0x0001;
#[allow(dead_code)]
pub const META_FLAG_REQUIRED: u32 = 0x0002;

pub fn section_name(id: u8) -> &'static str {
    match id {
        SECTION_METADATA => "Metadata",
        SECTION_BACKEND => "Backend",
        SECTION_FRONTEND => "Frontend",
        SECTION_WEBKIT => "Webkit",
        SECTION_ASSETS => "Assets",
        0xFF => "Reserved",
        _ => "Unknown",
    }
}

#[derive(Serialize, Deserialize, Debug)]
pub struct PluginMetadata {
    pub id: String,
    pub name: String,
    pub version: String,
    pub author: String,
    #[serde(default)]
    pub description: String,
    pub starlight_version: String,
    #[serde(default)]
    pub entry: String,
}

pub struct SubEntry {
    pub name: String,
    pub data: Vec<u8>,
}

pub fn serialize_sub_entries(entries: &[SubEntry]) -> Vec<u8> {
    let mut out = Vec::new();
    out.extend_from_slice(&(entries.len() as u32).to_le_bytes());
    for entry in entries {
        let name_bytes = entry.name.as_bytes();
        out.extend_from_slice(&(name_bytes.len() as u16).to_le_bytes());
        out.extend_from_slice(&(entry.data.len() as u32).to_le_bytes());
        out.extend_from_slice(name_bytes);
        out.extend_from_slice(&entry.data);
    }
    out
}

pub fn parse_sub_entries(blob: &[u8]) -> anyhow::Result<Vec<SubEntry>> {
    anyhow::ensure!(blob.len() >= 4, "sub-entry table too short");
    let file_count = u32::from_le_bytes(blob[0..4].try_into().unwrap()) as usize;
    let mut entries = Vec::with_capacity(file_count);
    let mut pos = 4;

    for i in 0..file_count {
        anyhow::ensure!(pos + 6 <= blob.len(), "sub-entry {} header truncated", i);
        let name_len = u16::from_le_bytes(blob[pos..pos + 2].try_into().unwrap()) as usize;
        let data_len = u32::from_le_bytes(blob[pos + 2..pos + 6].try_into().unwrap()) as usize;
        pos += 6;
        anyhow::ensure!(
            pos + name_len + data_len <= blob.len(),
            "sub-entry {} data truncated",
            i
        );
        let name = String::from_utf8(blob[pos..pos + name_len].to_vec())
            .map_err(|e| anyhow::anyhow!("sub-entry {} name not UTF-8: {}", i, e))?;
        let data = blob[pos + name_len..pos + name_len + data_len].to_vec();
        pos += name_len + data_len;
        entries.push(SubEntry { name, data });
    }
    Ok(entries)
}
