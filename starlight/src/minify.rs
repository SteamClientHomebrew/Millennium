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
    for line in out.lines() {
        let t = collapse_spaces(line.trim());
        if !t.is_empty() {
            result.push_str(&t);
            result.push('\n');
        }
    }

    result.into_bytes()
}

fn collapse_spaces(line: &str) -> String {
    let mut result = String::with_capacity(line.len());
    let mut in_str = false;
    let mut str_char = b'"';
    let mut prev_space = false;

    for b in line.bytes() {
        if in_str {
            result.push(b as char);
            if b == str_char {
                in_str = false;
            }
        } else if b == b'"' || b == b'\'' {
            in_str = true;
            str_char = b;
            prev_space = false;
            result.push(b as char);
        } else if b == b' ' || b == b'\t' {
            if !prev_space {
                result.push(' ');
            }
            prev_space = true;
        } else {
            prev_space = false;
            result.push(b as char);
        }
    }

    result
}
