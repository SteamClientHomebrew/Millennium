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
const PLG_PARITY_SEED: u32 = 0xA7C3E91F;
const PLG_STRIDE: usize = 64;

fn fnv1a(hash: u32, byte: u8) -> u32 {
    (hash ^ byte as u32).wrapping_mul(0x01000193)
}

fn fnv1a_u32(state: u32, value: u32) -> u32 {
    value.to_le_bytes().iter().fold(state, |h, &b| fnv1a(h, b))
}

pub fn weave_parity(data: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(data.len() + (data.len() / PLG_STRIDE + 1) * 10);
    let mut rolling: u32 = PLG_PARITY_SEED;
    let mut block_index: u16 = 0;

    for chunk in data.chunks(PLG_STRIDE) {
        out.extend_from_slice(chunk);
        let xor_check = chunk.iter().fold(0u32, |acc, &b| acc ^ b as u32);
        rolling = fnv1a_u32(rolling ^ xor_check, xor_check);
        out.extend_from_slice(&xor_check.to_le_bytes());
        out.extend_from_slice(&rolling.to_le_bytes());
        out.extend_from_slice(&block_index.to_le_bytes());
        block_index += 1;
    }
    out
}

pub fn verify_and_strip_parity(woven: &[u8]) -> anyhow::Result<Vec<u8>> {
    if woven.is_empty() {
        return Ok(Vec::new());
    }

    let mut out = Vec::new();
    let mut rolling: u32 = PLG_PARITY_SEED;
    let mut expected_index: u16 = 0;
    let mut pos = 0;

    while pos < woven.len() {
        let remaining = woven.len() - pos;
        anyhow::ensure!(
            remaining >= 10,
            "truncated parity stream at block {}",
            expected_index
        );

        let data_end = (pos + PLG_STRIDE).min(woven.len() - 10);
        anyhow::ensure!(
            data_end > pos,
            "malformed parity stream: zero-length chunk at block {}",
            expected_index
        );

        let chunk = &woven[pos..data_end];
        let parity_bytes = &woven[data_end..data_end + 10];

        let stored_xor = u32::from_le_bytes(parity_bytes[0..4].try_into().unwrap());
        let stored_roll = u32::from_le_bytes(parity_bytes[4..8].try_into().unwrap());
        let stored_index = u16::from_le_bytes(parity_bytes[8..10].try_into().unwrap());

        let xor_check = chunk.iter().fold(0u32, |acc, &b| acc ^ b as u32);
        let expected_roll = fnv1a_u32(rolling ^ xor_check, xor_check);

        anyhow::ensure!(
            stored_xor == xor_check,
            "parity xor mismatch at block {}",
            expected_index
        );
        anyhow::ensure!(
            stored_roll == expected_roll,
            "parity chain broken at block {}",
            expected_index
        );
        anyhow::ensure!(
            stored_index == expected_index,
            "parity block reordered at block {} (stored {})",
            expected_index,
            stored_index
        );

        out.extend_from_slice(chunk);
        rolling = expected_roll;
        expected_index += 1;
        pos = data_end + 10;
    }
    Ok(out)
}
