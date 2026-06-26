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
use anyhow::Context;

pub const MAGIC: &[u8; 4] = b"STAR";
pub const HEADER_SIZE: usize = 12;

pub const SECTION_ENTRY_SIZE: usize = 256;
pub const FORMAT_VERSION_MAJOR: u8 = 2;
pub const FORMAT_VERSION_MINOR: u8 = 0;

pub const STAR_FLAG_SIGNED: u8 = 0x01;

pub struct StarHeader {
    pub version_major: u8,
    pub version_minor: u8,
    pub section_count: u8,
    pub star_flags: u8,
    pub shim_integrity: u32,
}

impl StarHeader {
    pub fn new(section_count: u8, shim: &[u8]) -> Self {
        Self {
            version_major: FORMAT_VERSION_MAJOR,
            version_minor: FORMAT_VERSION_MINOR,
            section_count,
            star_flags: STAR_FLAG_SIGNED,
            shim_integrity: fnv1a(shim),
        }
    }

    pub fn to_bytes(&self) -> [u8; HEADER_SIZE] {
        let hash = self.shim_integrity.to_le_bytes();
        [
            MAGIC[0],
            MAGIC[1],
            MAGIC[2],
            MAGIC[3],
            self.version_major,
            self.version_minor,
            self.section_count,
            self.star_flags,
            hash[0],
            hash[1],
            hash[2],
            hash[3],
        ]
    }

    pub fn from_bytes(bytes: &[u8; HEADER_SIZE]) -> anyhow::Result<Self> {
        anyhow::ensure!(
            &bytes[0..4] == MAGIC.as_slice(),
            "invalid magic (not a .star file)"
        );
        Ok(Self {
            version_major: bytes[4],
            version_minor: bytes[5],
            section_count: bytes[6],
            star_flags: bytes[7],
            shim_integrity: u32::from_le_bytes(
                bytes[8..12].try_into().context("shim_integrity slice")?,
            ),
        })
    }
}

/// after a lot of thinking, this seems to be a good structure
/// 256-byte section entry (v2 format).
///
/// Layout:
///   0      u8    id
///   1      u8    encode_flags   (COMPRESSED=0x80, OBFUSCATED=0x40)
///   2–3    u8[2] pad            (zeroed)
///   4–7    u32   meta_flags     (DEFERRED=0x01, REQUIRED=0x02)
///   8–15   u64   offset         (bytes from star_start, LE)
///   16–23  u64   length         (encoded on-disk byte count, LE)
///   24–27  u32   crc32          (of encoded data)
///   28–255 u8[]  reserved       (zeroed; future fields go here)

pub struct SectionEntry {
    pub id: u8,
    pub encode_flags: u8,
    pub meta_flags: u32,
    pub offset: u64,
    pub length: u64,
    pub crc32: u32,
}

impl SectionEntry {
    pub fn to_bytes(&self) -> [u8; SECTION_ENTRY_SIZE] {
        let mut out = [0u8; SECTION_ENTRY_SIZE];
        out[0] = self.id;
        out[1] = self.encode_flags;
        // out[2..4] = pad, left as zero
        out[4..8].copy_from_slice(&self.meta_flags.to_le_bytes());
        out[8..16].copy_from_slice(&self.offset.to_le_bytes());
        out[16..24].copy_from_slice(&self.length.to_le_bytes());
        out[24..28].copy_from_slice(&self.crc32.to_le_bytes());
        // out[28..256] = reserved, left as zero
        out
    }

    pub fn from_bytes(bytes: &[u8; SECTION_ENTRY_SIZE]) -> Self {
        Self {
            id: bytes[0],
            encode_flags: bytes[1],
            meta_flags: u32::from_le_bytes(bytes[4..8].try_into().unwrap()),
            offset: u64::from_le_bytes(bytes[8..16].try_into().unwrap()),
            length: u64::from_le_bytes(bytes[16..24].try_into().unwrap()),
            crc32: u32::from_le_bytes(bytes[24..28].try_into().unwrap()),
        }
    }

    pub fn read_table(data: &[u8], count: usize) -> anyhow::Result<Vec<Self>> {
        anyhow::ensure!(
            data.len() >= count * SECTION_ENTRY_SIZE,
            "section table truncated: need {} bytes, got {}",
            count * SECTION_ENTRY_SIZE,
            data.len()
        );
        let mut entries = Vec::with_capacity(count);
        for i in 0..count {
            let start = i * SECTION_ENTRY_SIZE;
            let chunk: &[u8; SECTION_ENTRY_SIZE] = data[start..start + SECTION_ENTRY_SIZE]
                .try_into()
                .context("section entry slice")?;
            entries.push(Self::from_bytes(chunk));
        }
        Ok(entries)
    }
}

pub fn fnv1a(data: &[u8]) -> u32 {
    let mut h: u32 = 0x811c9dc5;
    for &b in data {
        h ^= b as u32;
        h = h.wrapping_mul(0x01000193);
    }
    h
}
