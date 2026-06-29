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
use regex::Regex;
use std::sync::OnceLock;

fn is_css_hash(hash: &str) -> bool {
    let len = hash.len();
    if len < 18 || len > 30 {
        return false;
    }
    if !hash
        .bytes()
        .all(|b| b.is_ascii_alphanumeric() || b == b'_' || b == b'-')
    {
        return false;
    }
    let has_digit = hash.bytes().any(|b| b.is_ascii_digit());
    let has_upper = hash.bytes().any(|b| b.is_ascii_uppercase());
    let has_consec_upper = hash
        .as_bytes()
        .windows(2)
        .any(|w| w[0].is_ascii_uppercase() && w[1].is_ascii_uppercase());
    let has_hyphen_mixed = hash.contains('-') && (has_digit || has_upper);

    has_digit || has_consec_upper || has_hyphen_mixed
}

static RE: OnceLock<Regex> = OnceLock::new();

fn re() -> &'static Regex {
    RE.get_or_init(|| {
        // 1: quoted key name (ex: "ItemFocusAnim-darkGrey")
        // 2: bare key name   (ex: coinFlip)
        // 3: hash value
        Regex::new(
            r#"(?:"([a-zA-Z_][a-zA-Z0-9_-]*)"|([a-zA-Z_][a-zA-Z0-9_]*)):"([a-zA-Z0-9_-]{18,30})""#,
        )
        .unwrap()
    })
}

pub fn patch(name: &str, content: &[u8]) -> Vec<u8> {
    let source = match std::str::from_utf8(content) {
        Ok(s) => s,
        Err(_) => return content.to_vec(),
    };

    let t0 = std::time::Instant::now();
    let mut count = 0usize;

    let result = re().replace_all(source, |caps: &regex::Captures| {
        let hash = &caps[3];
        if !is_css_hash(hash) {
            return caps[0].to_string();
        }
        count += 1;
        if let Some(qkey) = caps.get(1) {
            let key = qkey.as_str();
            format!("\"{key}\":\"{hash} {key}\"")
        } else {
            let key = &caps[2];
            format!("{key}:\"{hash} {key}\"")
        }
    });

    log::info!(
        "{}: css reclass: {} class(es) in {:.2}ms",
        name,
        count,
        t0.elapsed().as_secs_f64() * 1000.0,
    );

    result.into_owned().into_bytes()
}
