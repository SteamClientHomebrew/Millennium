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
#[derive(Debug, Clone, PartialEq)]
pub enum FfiType {
    Number,
    String,
    Boolean,
    Nil,
    Unknown,
    Void,
    Array(Box<FfiType>),
    Union(Vec<FfiType>),
    Object,
}

impl std::fmt::Display for FfiType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.to_ts())
    }
}

impl FfiType {
    pub fn to_ts(&self) -> String {
        match self {
            Self::Number => "number".into(),
            Self::String => "string".into(),
            Self::Boolean => "boolean".into(),
            Self::Nil => "null".into(),
            Self::Unknown => "unknown".into(),
            Self::Void => "void".into(),
            Self::Array(inner) => format!("{}[]", inner.to_ts()),
            Self::Union(types) => types
                .iter()
                .map(|t| t.to_ts())
                .collect::<Vec<_>>()
                .join(" | "),
            Self::Object => "Record<string, unknown>".into(),
        }
    }

    pub fn compatible_with(&self, other: &Self) -> bool {
        use FfiType::*;
        match (self, other) {
            (Unknown, _) | (_, Unknown) => true,
            (Void, Nil) | (Nil, Void) => true,
            (a, b) if a == b => true,
            (Array(a), Array(b)) => a.compatible_with(b),
            (Union(parts), other) => parts.iter().any(|t| t.compatible_with(other)),
            (this, Union(parts)) => parts.iter().any(|t| this.compatible_with(t)),
            _ => false,
        }
    }
}

pub fn parse_lua_type(s: &str) -> FfiType {
    let s = s.trim().trim_end_matches('?');
    if s.contains('|') {
        return FfiType::Union(s.split('|').map(|p| parse_lua_type(p.trim())).collect());
    }
    match s {
        "number" | "integer" | "float" | "int" => FfiType::Number,
        "string" => FfiType::String,
        "boolean" | "bool" => FfiType::Boolean,
        "nil" => FfiType::Nil,
        "any" | "unknown" => FfiType::Unknown,
        "void" => FfiType::Void,
        "table" => FfiType::Object,
        s if s.ends_with("[]") => FfiType::Array(Box::new(parse_lua_type(&s[..s.len() - 2]))),
        _ => FfiType::Unknown,
    }
}

pub fn ffi_type_from_ts<'a>(ty: &oxc_ast::ast::TSType<'a>) -> FfiType {
    use oxc_ast::ast::TSType;
    match ty {
        TSType::TSNumberKeyword(_) => FfiType::Number,
        TSType::TSStringKeyword(_) => FfiType::String,
        TSType::TSBooleanKeyword(_) => FfiType::Boolean,
        TSType::TSNullKeyword(_) | TSType::TSUndefinedKeyword(_) => FfiType::Nil,
        TSType::TSVoidKeyword(_) => FfiType::Void,
        TSType::TSAnyKeyword(_) | TSType::TSUnknownKeyword(_) | TSType::TSNeverKeyword(_) => {
            FfiType::Unknown
        }
        TSType::TSArrayType(t) => FfiType::Array(Box::new(ffi_type_from_ts(&t.element_type))),
        TSType::TSUnionType(t) => FfiType::Union(t.types.iter().map(ffi_type_from_ts).collect()),
        TSType::TSObjectKeyword(_) | TSType::TSTypeLiteral(_) => FfiType::Object,
        _ => FfiType::Unknown,
    }
}
