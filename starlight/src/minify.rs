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
pub fn minify_js(code: &str) -> String {
    use oxc_allocator::Allocator;
    use oxc_codegen::{Codegen, CodegenOptions};
    use oxc_minifier::{Minifier, MinifierOptions};
    use oxc_parser::Parser;
    use oxc_span::SourceType;

    let allocator = Allocator::default();
    let ret = Parser::new(&allocator, code, SourceType::mjs()).parse();
    if !ret.errors.is_empty() {
        crate::log::warn("JS minification skipped: parse errors in transformed bundle");
        return code.to_owned();
    }
    let mut program = ret.program;
    let ret = Minifier::new(MinifierOptions::default()).minify(&allocator, &mut program);
    Codegen::new()
        .with_options(CodegenOptions::minify())
        .with_scoping(ret.scoping)
        .build(&program)
        .code
}

pub fn minify_lua(src: &[u8]) -> Vec<u8> {
    let Ok(src) = std::str::from_utf8(src) else {
        return src.to_vec();
    };

    let b = src.as_bytes();
    let len = b.len();
    let mut out = String::with_capacity(len / 2);
    let mut i = 0usize;

    while i < len {
        if i + 1 < len && b[i] == b'-' && b[i + 1] == b'-' {
            i += 2;
            if i < len && b[i] == b'[' {
                let eq_start = i + 1;
                let mut j = eq_start;
                while j < len && b[j] == b'=' {
                    j += 1;
                }
                if j < len && b[j] == b'[' {
                    let level = j - eq_start;
                    i = j + 1;
                    'blk: loop {
                        if i >= len {
                            break;
                        }
                        if b[i] == b']' {
                            let mut k = i + 1;
                            let mut cl = 0usize;
                            while k < len && b[k] == b'=' {
                                cl += 1;
                                k += 1;
                            }
                            if k < len && b[k] == b']' && cl == level {
                                i = k + 1;
                                break 'blk;
                            }
                        }
                        i += 1;
                    }
                    continue;
                }
            }
            while i < len && b[i] != b'\n' {
                i += 1;
            }
            continue;
        }

        if b[i] == b'"' || b[i] == b'\'' {
            let quote = b[i];
            out.push(b[i] as char);
            i += 1;
            while i < len {
                if b[i] == b'\\' {
                    out.push('\\');
                    i += 1;
                    if i < len {
                        out.push(b[i] as char);
                        i += 1;
                    }
                    continue;
                }
                out.push(b[i] as char);
                let done = b[i] == quote;
                i += 1;
                if done {
                    break;
                }
            }
            continue;
        }

        if b[i] == b'[' {
            let eq_start = i + 1;
            let mut j = eq_start;
            while j < len && b[j] == b'=' {
                j += 1;
            }
            if j < len && b[j] == b'[' {
                let level = j - eq_start;
                out.push_str(&src[i..=j]);
                i = j + 1;
                'lstr: loop {
                    if i >= len {
                        break;
                    }
                    if b[i] == b']' {
                        let mut k = i + 1;
                        let mut cl = 0usize;
                        while k < len && b[k] == b'=' {
                            cl += 1;
                            k += 1;
                        }
                        if k < len && b[k] == b']' && cl == level {
                            out.push_str(&src[i..=k]);
                            i = k + 1;
                            break 'lstr;
                        }
                    }
                    out.push(b[i] as char);
                    i += 1;
                }
                continue;
            }
        }

        out.push(b[i] as char);
        i += 1;
    }

    let mut result = String::with_capacity(out.len());
    let ob = out.as_bytes();
    let olen = ob.len();
    let mut ip = 0usize;
    let mut at_line_start = true;
    let mut line_has_content = false;
    let mut pending_space = false;

    while ip < olen {
        let c = ob[ip];

        if c == b'[' {
            let eq_start = ip + 1;
            let mut j = eq_start;
            while j < olen && ob[j] == b'=' {
                j += 1;
            }
            if j < olen && ob[j] == b'[' {
                let level = j - eq_start;
                if pending_space && line_has_content {
                    result.push(' ');
                    pending_space = false;
                }
                result.push_str(&out[ip..=j]);
                line_has_content = true;
                at_line_start = false;
                ip = j + 1;
                'lstr2: loop {
                    if ip >= olen {
                        break;
                    }
                    if ob[ip] == b']' {
                        let mut k = ip + 1;
                        let mut cl = 0usize;
                        while k < olen && ob[k] == b'=' {
                            cl += 1;
                            k += 1;
                        }
                        if k < olen && ob[k] == b']' && cl == level {
                            result.push_str(&out[ip..=k]);
                            ip = k + 1;
                            break 'lstr2;
                        }
                    }
                    result.push(ob[ip] as char);
                    ip += 1;
                }
                continue;
            }
        }

        if c == b'"' || c == b'\'' {
            if pending_space && line_has_content {
                result.push(' ');
                pending_space = false;
            }
            let quote = c;
            result.push(c as char);
            line_has_content = true;
            at_line_start = false;
            ip += 1;
            while ip < olen {
                let qc = ob[ip];
                if qc == b'\\' {
                    result.push('\\');
                    ip += 1;
                    if ip < olen {
                        result.push(ob[ip] as char);
                        ip += 1;
                    }
                    continue;
                }
                result.push(qc as char);
                let done = qc == quote;
                ip += 1;
                if done {
                    break;
                }
            }
            continue;
        }

        if c == b'\n' {
            if line_has_content {
                result.push('\n');
            }
            at_line_start = true;
            line_has_content = false;
            pending_space = false;
            ip += 1;
            continue;
        }

        if c == b' ' || c == b'\t' {
            if at_line_start {
                ip += 1;
                continue;
            }
            pending_space = true;
            ip += 1;
            continue;
        }

        at_line_start = false;
        if pending_space {
            result.push(' ');
            pending_space = false;
        }
        result.push(c as char);
        line_has_content = true;
        ip += 1;
    }

    result.into_bytes()
}
