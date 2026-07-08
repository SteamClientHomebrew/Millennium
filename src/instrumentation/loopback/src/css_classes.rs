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

fn is_css_hash(hash: &[u8]) -> bool {
    let len = hash.len();
    if len < 18 || len > 30 {
        return false;
    }
    if !hash
        .iter()
        .all(|&b| b.is_ascii_alphanumeric() || b == b'_' || b == b'-')
    {
        return false;
    }
    let has_digit = hash.iter().any(|b| b.is_ascii_digit());
    let has_consec_upper = hash
        .windows(2)
        .any(|w| w[0].is_ascii_uppercase() && w[1].is_ascii_uppercase());
    let has_upper = hash.iter().any(|b| b.is_ascii_uppercase());
    let has_hyphen_mixed = hash.contains(&b'-') && (has_digit || has_upper);

    has_digit || has_consec_upper || has_hyphen_mixed
}

#[inline]
fn is_key_start(c: u8) -> bool {
    c.is_ascii_alphabetic() || c == b'_'
}
#[inline]
fn is_key_cont(c: u8, hyphen_ok: bool) -> bool {
    c.is_ascii_alphanumeric() || c == b'_' || (hyphen_ok && c == b'-')
}
#[inline]
fn is_hash_char(c: u8) -> bool {
    c.is_ascii_alphanumeric() || c == b'_' || c == b'-'
}
#[inline]
fn js_ident_start(c: u8) -> bool {
    c.is_ascii_alphabetic() || c == b'_' || c == b'$'
}
#[inline]
fn js_ident_cont(c: u8) -> bool {
    c.is_ascii_alphanumeric() || c == b'_' || c == b'$'
}

fn skip_ws(src: &[u8], mut p: usize) -> usize {
    while p < src.len() && matches!(src[p], b' ' | b'\t' | b'\n' | b'\r') {
        p += 1;
    }
    p
}

fn match_exports_open(src: &[u8], p: usize) -> Option<usize> {
    let n = src.len();
    if p == 0 || !js_ident_cont(src[p - 1]) {
        return None;
    }
    if p + 8 > n || &src[p..p + 8] != b".exports" {
        return None;
    }
    let mut q = skip_ws(src, p + 8);
    if q >= n || src[q] != b'=' {
        return None;
    }
    q += 1;
    if q < n && src[q] == b'=' {
        return None; // == or ===
    }
    q = skip_ws(src, q);
    if q >= n || src[q] != b'{' {
        return None;
    }
    Some(q + 1)
}

fn patch_css_classes(src: &[u8]) -> (Vec<u8>, usize) {
    let n = src.len();
    let mut out = Vec::with_capacity(n + n / 4 + 4096);
    let mut flush = 0usize;
    let mut count = 0usize;
    let mut p = 0usize;
    let mut in_block = false;

    while p < n {
        if !in_block {
            let Some(rel) = src[p..].iter().position(|&b| b == b'.') else {
                break;
            };
            p += rel;
            match match_exports_open(src, p) {
                Some(after) => {
                    p = after;
                    in_block = true;
                }
                None => p += 1,
            }
            continue;
        }

        if src[p] == b'}' {
            in_block = false;
            p += 1;
            continue;
        }

        let match_start = p;
        let key_start;
        let key_len;
        let is_quoted;

        if src[p] == b'"' {
            is_quoted = true;
            let mut q = p + 1;
            if q >= n || !is_key_start(src[q]) {
                p += 1;
                continue;
            }
            q += 1;
            while q < n && is_key_cont(src[q], true) {
                q += 1;
            }
            if q >= n || src[q] != b'"' {
                p = match_start + 1;
                continue;
            }
            key_start = p + 1;
            key_len = q - key_start;
            p = q + 1;
        } else if is_key_start(src[p]) {
            is_quoted = false;
            key_start = p;
            p += 1;
            while p < n && is_key_cont(src[p], false) {
                p += 1;
            }
            key_len = p - key_start;
        } else {
            p += 1;
            continue;
        }

        if p + 2 > n || src[p] != b':' || src[p + 1] != b'"' {
            if is_quoted {
                p = match_start + 1;
            }
            continue;
        }
        p += 2;

        let hash_start = p;
        while p < n && is_hash_char(src[p]) {
            p += 1;
        }
        let hash_len = p - hash_start;

        if hash_len < 18 || hash_len > 30 || p >= n || src[p] != b'"' {
            continue;
        }
        p += 1;

        if !is_css_hash(&src[hash_start..hash_start + hash_len]) {
            continue;
        }

        out.extend_from_slice(&src[flush..match_start]);
        let key = &src[key_start..key_start + key_len];
        let hash = &src[hash_start..hash_start + hash_len];
        if is_quoted {
            out.push(b'"');
            out.extend_from_slice(key);
            out.extend_from_slice(b"\":\"");
            out.extend_from_slice(hash);
            out.push(b' ');
            out.extend_from_slice(key);
            out.push(b'"');
        } else {
            out.extend_from_slice(key);
            out.extend_from_slice(b":\"");
            out.extend_from_slice(hash);
            out.push(b' ');
            out.extend_from_slice(key);
            out.push(b'"');
        }
        flush = p;
        count += 1;
    }

    out.extend_from_slice(&src[flush..]);
    (out, count)
}

#[inline]
fn is_recv_ident_char(c: u8) -> bool {
    c.is_ascii_alphanumeric() || c == b'_' || c == b'$'
}

fn scan_receiver_start(src: &[u8], dot_pos: usize) -> Option<usize> {
    const MAX_SCAN: usize = 512;
    let scan_floor = dot_pos.saturating_sub(MAX_SCAN);
    let mut cur_dot = dot_pos;

    loop {
        let connector_start = if cur_dot > 0 && src[cur_dot - 1] == b'?' {
            cur_dot - 1
        } else {
            cur_dot
        };
        if connector_start == 0 || connector_start < scan_floor {
            return None;
        }

        let mut k = connector_start;
        while k > 0 && is_recv_ident_char(src[k - 1]) {
            k -= 1;
        }
        if k == connector_start {
            return None;
        }

        if k > 0 && src[k - 1] == b'.' {
            cur_dot = k - 1;
            continue;
        }

        if !(src[k].is_ascii_alphabetic() || src[k] == b'_' || src[k] == b'$') {
            return None;
        }
        return Some(k);
    }
}

fn patch_classlist(src: &[u8]) -> (Vec<u8>, usize) {
    let mut out = Vec::with_capacity(src.len() + src.len() / 8 + 4096);
    let mut count = 0usize;
    let n = src.len();
    let mut p = 0usize;
    let mut flush = 0usize;

    enum Kind {
        Add,
        Remove,
        Toggle,
        Contains,
    }

    while p < n {
        let rel = match src[p..].iter().position(|&b| b == b'c') {
            Some(r) => r,
            None => break,
        };
        p += rel;

        let (kind, method_start, after_paren) =
            if p + 14 <= n && &src[p..p + 14] == b"classList.add(" {
                (Kind::Add, p, p + 14)
            } else if p + 17 <= n && &src[p..p + 17] == b"classList.remove(" {
                (Kind::Remove, p, p + 17)
            } else if p + 17 <= n && &src[p..p + 17] == b"classList.toggle(" {
                (Kind::Toggle, p, p + 17)
            } else if p + 19 <= n && &src[p..p + 19] == b"classList.contains(" {
                (Kind::Contains, p, p + 19)
            } else {
                p += 1;
                continue;
            };

        let mut q = after_paren;
        if q >= n || !js_ident_start(src[q]) {
            p += 1;
            continue;
        }
        let mod_start = q;
        while q < n && js_ident_cont(src[q]) {
            q += 1;
        }
        if q + 1 < n && src[q] == b'(' && src[q + 1] == b')' {
            q += 2;
        }
        if q >= n || src[q] != b'.' {
            p += 1;
            continue;
        }
        q += 1;
        if q >= n || !js_ident_start(src[q]) {
            p += 1;
            continue;
        }
        while q < n && js_ident_cont(src[q]) {
            q += 1;
        }
        let expr_end = q;
        if q >= n || src[q] != b')' {
            p += 1;
            continue;
        }
        q += 1;

        match kind {
            Kind::Add | Kind::Remove => {
                out.extend_from_slice(&src[flush..after_paren]);
                out.extend_from_slice(b"...");
                out.extend_from_slice(&src[mod_start..expr_end]);
                out.extend_from_slice(b".split(\" \")");
                out.push(b')');
            }
            Kind::Contains => {
                out.extend_from_slice(&src[flush..after_paren]);
                out.extend_from_slice(&src[mod_start..expr_end]);
                out.extend_from_slice(b".split(\" \")[0]");
                out.push(b')');
            }
            Kind::Toggle => {
                if method_start == 0 || src[method_start - 1] != b'.' {
                    p = q;
                    continue;
                }
                let Some(recv_start) = scan_receiver_start(src, method_start - 1) else {
                    p = q;
                    continue;
                };

                out.extend_from_slice(&src[flush..recv_start]);
                out.extend_from_slice(b"((__milTgC)=>{const __milTgS=(");
                out.extend_from_slice(&src[mod_start..expr_end]);
                out.extend_from_slice(
                    b").split(\" \");const __milTgH=__milTgC.contains(__milTgS[0]);__milTgH?__milTgC.remove(...__milTgS):__milTgC.add(...__milTgS);return!__milTgH;})(",
                );
                out.extend_from_slice(&src[recv_start..method_start]);
                out.extend_from_slice(b"classList)");
            }
        }

        flush = q;
        p = q;
        count += 1;
    }

    out.extend_from_slice(&src[flush..]);
    (out, count)
}

pub fn patch(name: &str, content: &[u8]) -> Vec<u8> {
    let t0 = std::time::Instant::now();

    let (patched, count) = patch_css_classes(content);
    let (patched, cl_count) = patch_classlist(&patched);

    log::info!(
        "{}: resident: {} class(es) rewritten, {} classList call(s) patched in {:.2}ms",
        name,
        count,
        cl_count,
        t0.elapsed().as_secs_f64() * 1000.0,
    );

    patched
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn hash_accepts_digit() {
        assert!(is_css_hash(b"ioCYZzrMW1sGR7VpkN806"));
    }
    #[test]
    fn hash_rejects_all_lowercase() {
        assert!(!is_css_hash(b"abcdefghijklmnopqrstu"));
    }
    #[test]
    fn hash_rejects_too_short() {
        assert!(!is_css_hash(b"ABC1"));
    }
    #[test]
    fn hash_rejects_too_long() {
        assert!(!is_css_hash(b"abcdefghijklmnopqrstuvwxyz12345"));
    }
    #[test]
    fn hash_accepts_consec_upper() {
        assert!(is_css_hash(b"FocusNavigationRootABcd"));
    }
    #[test]
    fn hash_accepts_hyphen_with_upper() {
        assert!(is_css_hash(b"ItemFocusAnim-darkGrey1234"));
    }
    #[test]
    fn css_classes_bare_key() {
        let (out, count) = patch_css_classes(b"e.exports={coinFlip:\"ioCYZzrMW1sGR7VpkN806\"}");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"e.exports={coinFlip:\"ioCYZzrMW1sGR7VpkN806 coinFlip\"}"
        );
    }
    #[test]
    fn css_classes_quoted_key() {
        let (out, count) =
            patch_css_classes(b"e.exports={\"ItemFocusAnim-darkGrey\":\"ioCYZzrMW1sGR7VpkN806\"}");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"e.exports={\"ItemFocusAnim-darkGrey\":\"ioCYZzrMW1sGR7VpkN806 ItemFocusAnim-darkGrey\"}"
        );
    }
    #[test]
    fn css_classes_skips_non_hash() {
        let (out, count) = patch_css_classes(b"e.exports={key:\"tooshort\"}");
        assert_eq!(count, 0);
        assert_eq!(out, b"e.exports={key:\"tooshort\"}");
    }
    #[test]
    fn css_classes_skips_all_lowercase_value() {
        let (out, count) = patch_css_classes(b"e.exports={key:\"abcdefghijklmnopqrstu\"}");
        assert_eq!(count, 0);
        assert_eq!(out, b"e.exports={key:\"abcdefghijklmnopqrstu\"}");
    }
    #[test]
    fn css_classes_multiple_matches() {
        let input = b"e.exports={a:\"ioCYZzrMW1sGR7VpkN806\",b:\"FocusNavigationRootABcd\"}";
        let (out, count) = patch_css_classes(input);
        assert_eq!(count, 2);
        assert!(out.starts_with(b"e.exports={a:\"ioCYZzrMW1sGR7VpkN806 a\""));
    }
    #[test]
    fn css_classes_multiple_export_blocks() {
        let input =
            b"a.exports={x:\"ioCYZzrMW1sGR7VpkN806\"};b.exports={y:\"FocusNavigationRootABcd\"}";
        let (_, count) = patch_css_classes(input);
        assert_eq!(count, 2);
    }
    #[test]
    fn css_classes_idempotent_on_already_patched() {
        let input = b"e.exports={coinFlip:\"ioCYZzrMW1sGR7VpkN806 coinFlip\"}";
        let (out, count) = patch_css_classes(input);
        assert_eq!(count, 0);
        assert_eq!(out, input.to_vec());
    }
    #[test]
    fn css_classes_skips_outside_exports() {
        let input = b"menuItems=[{urlName:\"SteamIDFriendsPage\"}]";
        let (out, count) = patch_css_classes(input);
        assert_eq!(count, 0);
        assert_eq!(out, input.to_vec());
    }
    #[test]
    fn css_classes_skips_equality_operator() {
        let input = b"if(e.exports=={}){}";
        let (out, count) = patch_css_classes(input);
        assert_eq!(count, 0);
        assert_eq!(out, input.to_vec());
    }
    #[test]
    fn css_classes_exports_with_spaces() {
        let (out, count) = patch_css_classes(b"e.exports = {coinFlip:\"ioCYZzrMW1sGR7VpkN806\"}");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"e.exports = {coinFlip:\"ioCYZzrMW1sGR7VpkN806 coinFlip\"}"
        );
    }
    #[test]
    fn classlist_factory_call_remove() {
        let (out, count) = patch_classlist(b"e.current?.classList.remove(We().ErrorShake)");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"e.current?.classList.remove(...We().ErrorShake.split(\" \"))"
        );
    }
    #[test]
    fn classlist_factory_call_add() {
        let (out, count) = patch_classlist(b"n.classList.add(We().ErrorShake)");
        assert_eq!(count, 1);
        assert_eq!(out, b"n.classList.add(...We().ErrorShake.split(\" \"))");
    }
    #[test]
    fn classlist_direct_property() {
        let (out, count) = patch_classlist(b"el.classList.add(styles.focusRing)");
        assert_eq!(count, 1);
        assert_eq!(out, b"el.classList.add(...styles.focusRing.split(\" \"))");
    }
    #[test]
    fn classlist_dollar_sign_ident() {
        let (out, count) = patch_classlist(b"el.classList.add($s.focusRing)");
        assert_eq!(count, 1);
        assert_eq!(out, b"el.classList.add(...$s.focusRing.split(\" \"))");
    }
    #[test]
    fn classlist_no_match_complex_arg() {
        let input = b"el.classList.add(computeClass(x))";
        let (out, count) = patch_classlist(input);
        assert_eq!(count, 0);
        assert_eq!(out, input.to_vec());
    }
    #[test]
    fn classlist_no_match_string_arg() {
        let input = b"el.classList.add(\"someClass\")";
        let (out, count) = patch_classlist(input);
        assert_eq!(count, 0);
        assert_eq!(out, input.to_vec());
    }
    #[test]
    fn classlist_multiple_calls() {
        let input = b"a.classList.add(s.foo);b.classList.remove(t().bar)";
        let (out, count) = patch_classlist(input);
        assert_eq!(count, 2);
        assert_eq!(
            out,
            b"a.classList.add(...s.foo.split(\" \"));b.classList.remove(...t().bar.split(\" \"))"
        );
    }
    #[test]
    fn classlist_contains_rewrite() {
        let (out, count) = patch_classlist(b"el.classList.contains(styles.focusRing)");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"el.classList.contains(styles.focusRing.split(\" \")[0])"
        );
    }
    #[test]
    fn classlist_contains_factory_call() {
        let (out, count) = patch_classlist(b"n.classList.contains(We().ErrorShake)");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"n.classList.contains(We().ErrorShake.split(\" \")[0])"
        );
    }
    #[test]
    fn classlist_toggle_simple_receiver() {
        let (out, count) = patch_classlist(b"el.classList.toggle(styles.focusRing)");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"((__milTgC)=>{const __milTgS=(styles.focusRing).split(\" \");const __milTgH=__milTgC.contains(__milTgS[0]);__milTgH?__milTgC.remove(...__milTgS):__milTgC.add(...__milTgS);return!__milTgH;})(el.classList)"
        );
    }
    #[test]
    fn classlist_toggle_optional_chaining_receiver() {
        let (out, count) = patch_classlist(b"e.current?.classList.toggle(We().ErrorShake)");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"((__milTgC)=>{const __milTgS=(We().ErrorShake).split(\" \");const __milTgH=__milTgC.contains(__milTgS[0]);__milTgH?__milTgC.remove(...__milTgS):__milTgC.add(...__milTgS);return!__milTgH;})(e.current?.classList)"
        );
    }
    #[test]
    fn classlist_toggle_dotted_chain_receiver() {
        let (out, count) = patch_classlist(b"e.current.classList.toggle(s.foo)");
        assert_eq!(count, 1);
        assert_eq!(
            out,
            b"((__milTgC)=>{const __milTgS=(s.foo).split(\" \");const __milTgH=__milTgC.contains(__milTgS[0]);__milTgH?__milTgC.remove(...__milTgS):__milTgC.add(...__milTgS);return!__milTgH;})(e.current.classList)"
        );
    }
    #[test]
    fn classlist_toggle_bails_on_call_expr_receiver() {
        let input = b"document.querySelector(x).classList.toggle(s.foo)";
        let (out, count) = patch_classlist(input);
        assert_eq!(count, 0);
        assert_eq!(out, input.to_vec());
    }
    #[test]
    fn classlist_toggle_preserves_add_remove_around_it() {
        let input = b"a.classList.add(s.foo);b.classList.toggle(t.bar);c.classList.remove(u.baz)";
        let (out, count) = patch_classlist(input);
        assert_eq!(count, 3);
        assert_eq!(
            out,
            b"a.classList.add(...s.foo.split(\" \"));((__milTgC)=>{const __milTgS=(t.bar).split(\" \");const __milTgH=__milTgC.contains(__milTgS[0]);__milTgH?__milTgC.remove(...__milTgS):__milTgC.add(...__milTgS);return!__milTgH;})(b.classList);c.classList.remove(...u.baz.split(\" \"))"
        );
    }
}
