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
use ed25519_dalek::{Signature, Signer, SigningKey, Verifier, VerifyingKey};

fn decode_hex_32(s: &str) -> anyhow::Result<[u8; 32]> {
    let s = s.trim();
    anyhow::ensure!(
        s.len() == 64,
        "STARLIGHT_SIGNING_KEY must be 64 hex chars (32 bytes)"
    );
    let mut out = [0u8; 32];
    for i in 0..32 {
        out[i] = u8::from_str_radix(&s[i * 2..i * 2 + 2], 16)
            .map_err(|_| anyhow::anyhow!("invalid hex at position {}", i * 2))?;
    }
    Ok(out)
}

fn load_signing_key() -> anyhow::Result<SigningKey> {
    let hex = std::env::var("STARLIGHT_SIGNING_KEY").map_err(|_| {
        anyhow::anyhow!(
            "STARLIGHT_SIGNING_KEY is not set.\n\
             Set it to the 64-char hex private key seed before packing."
        )
    })?;
    Ok(SigningKey::from_bytes(&decode_hex_32(&hex)?))
}

pub fn try_sign_star(data: &[u8]) -> anyhow::Result<Option<[u8; 64]>> {
    if std::env::var("STARLIGHT_SIGNING_KEY").is_err() {
        return Ok(None);
    }
    let key = load_signing_key()?;
    let sig: Signature = key.sign(data);
    Ok(Some(sig.to_bytes()))
}

pub fn verify_star(data: &[u8], signature: &[u8; 64], public_key: &[u8; 32]) -> bool {
    let Ok(vk) = VerifyingKey::from_bytes(public_key) else {
        return false;
    };
    let sig = Signature::from(*signature);
    vk.verify(data, &sig).is_ok()
}

pub fn get_verifying_key_bytes() -> anyhow::Result<[u8; 32]> {
    Ok(load_signing_key()?.verifying_key().to_bytes())
}
