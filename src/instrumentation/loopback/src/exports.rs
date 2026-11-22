use oxc_allocator::Allocator;
use oxc_ast::ast::{Expression, ObjectPropertyKind, PropertyKey, Statement};
use oxc_parser::Parser;
use oxc_span::{GetSpan, SourceType};
use regex::Regex;
use std::collections::HashMap;
use std::sync::OnceLock;

use crate::wrappers;

const INTERNAL_PREFIX: &str = "__millennium_internal_";
const CLASS_MAP_PREFIX: &str = "__millennium_classmap_";
const MIN_CLASS_MAP_PROPERTIES: usize = 3;
const MIN_HASH_CONSTANTS: usize = 20;

struct InternalDef {
    name: &'static str,
    needle: &'static str,
    pattern: &'static str,
}

const INTERNALS: &[InternalDef] = &[
    InternalDef {
        name: "ProgressBar",
        needle: "{let{nProgress:",
        pattern: r"^function [\w$]+\([\w$]+\)\{let\{nProgress:[\w$]+,nTransitionSec:[\w$]+=1,margin:",
    },
    InternalDef {
        name: "ProgressBarItem",
        needle: "nTransitionSec:",
        pattern: r"\{indeterminate:[\w$]+,nTransitionSec:[\w$]+,nProgress:[\w$]+,childrenContainerWidth:",
    },
    InternalDef {
        name: "Marquee",
        needle: "e.play??!0",
        pattern: r"e\.play\?\?!0.*e\.direction\?\?`left`",
    },
    InternalDef {
        name: "SteamSpinner",
        needle: "strMovieUrl",
        pattern: r"background:[\w$]+\}=e;.*strMovieUrl",
    },
    InternalDef {
        name: "SidebarNavigation",
        needle: "fnSetNavigateToPage",
        pattern: r"\{pages:[\w$]+,fnSetNavigateToPage:[\w$]+,disableRouteReporting:",
    },
    InternalDef {
        name: "Route",
        needle: "routePath:",
        pattern: r"routePath:[\w$]+\.match\?\.path",
    },
    InternalDef {
        name: "BBCodeParser",
        needle: "static ElementAccumulator()",
        pattern: r"^class [\w$]+ extends [\w$]+\.Component\{parser;constructor\([\w$]+\)\{super\([\w$]+\),this\.parser=new [\w$]+\([\w$]+\.BBCodeDictionary,[\w$]+\.ElementAccumulator,",
    },
    InternalDef {
        name: "PanelSection",
        needle: ".spinner&&(0,",
        pattern: r"^function [\w$]+\([\w$]+\)\{let\{title:[\w$]+,children:[\w$]+\}=[\w$]+;return\(0,[\w$]+\.jsxs\)\(`div`,\{className:[\w$]+,children:\[[\w$]+&&\(0,[\w$]+\.jsxs\)\(`div`,\{className:`[\w$-]+`,children:\[\(0,[\w$]+\.jsx\)\(`div`,\{className:`[\w$-]+`,children:[\w$]+\}\),[\w$]+\.spinner&&",
    },
];

const PANEL_SECTION: &str = "PanelSection";
const PANEL_SECTION_ROW: &str = "PanelSectionRow";

fn internal_patterns() -> &'static Vec<Regex> {
    static PATTERNS: OnceLock<Vec<Regex>> = OnceLock::new();
    PATTERNS.get_or_init(|| INTERNALS.iter().map(|def| Regex::new(def.pattern).expect("internal pattern must compile")).collect())
}

fn class_hash() -> &'static Regex {
    static HASH: OnceLock<Regex> = OnceLock::new();
    HASH.get_or_init(|| Regex::new(r"^[A-Za-z0-9_-]{11,12}-$").expect("class hash pattern must compile"))
}

fn hash_literal_counter() -> &'static Regex {
    static COUNTER: OnceLock<Regex> = OnceLock::new();
    COUNTER.get_or_init(|| Regex::new(r"`[A-Za-z0-9_-]{11,12}-`").expect("hash literal pattern must compile"))
}

fn first_class_token(value: &str) -> Option<&str> {
    let token = value.split(' ').next()?;
    class_hash().is_match(token).then_some(token)
}

fn string_value<'a>(expression: &'a Expression<'a>) -> Option<&'a str> {
    match expression {
        Expression::StringLiteral(literal) => Some(literal.value.as_str()),
        Expression::TemplateLiteral(template) if template.expressions.is_empty() => {
            template.quasis.first().and_then(|quasi| quasi.value.cooked.as_ref()).map(|cooked| cooked.as_str())
        }
        _ => None,
    }
}

fn has_candidate(text: &str) -> bool {
    if INTERNALS.iter().any(|def| text.contains(def.needle)) {
        return true;
    }
    wrappers::has_candidate(text) || hash_literal_counter().find_iter(text).take(MIN_HASH_CONSTANTS).count() >= MIN_HASH_CONSTANTS
}

struct Found {
    binding: String,
    internal: usize,
}

struct Analysis {
    found: Vec<Found>,
    class_maps: Vec<String>,
    extra: Vec<(String, String)>,
    wrappers: Vec<wrappers::Wrapper>,
    react: Option<String>,
}

fn analyse(text: &str) -> Result<Analysis, String> {
    let allocator = Allocator::default();
    let parsed = Parser::new(&allocator, text, SourceType::mjs()).parse();
    if parsed.panicked || !parsed.errors.is_empty() {
        let first = parsed.errors.first().map(|error| error.to_string()).unwrap_or_else(|| "parser panicked".to_string());
        return Err(format!("{} error(s), first: {}", parsed.errors.len(), first));
    }

    let mut constants: HashMap<&str, &str> = HashMap::new();
    for statement in &parsed.program.body {
        let Statement::VariableDeclaration(declaration) = statement else { continue };
        for declarator in &declaration.declarations {
            let (Some(name), Some(init)) = (declarator.id.get_identifier_name(), declarator.init.as_ref()) else { continue };
            if let Some(hash) = string_value(init).and_then(first_class_token) {
                constants.insert(name.as_str(), hash);
            }
        }
    }

    let patterns = internal_patterns();
    let mut matches: Vec<Vec<String>> = vec![Vec::new(); INTERNALS.len()];
    let mut class_maps: Vec<String> = Vec::new();

    let mut consider = |binding: &str, source: &str| {
        for (index, def) in INTERNALS.iter().enumerate() {
            if source.contains(def.needle) && patterns[index].is_match(source) {
                matches[index].push(binding.to_string());
            }
        }
    };

    for statement in &parsed.program.body {
        match statement {
            Statement::FunctionDeclaration(function) => {
                if let Some(id) = &function.id {
                    consider(id.name.as_str(), &text[function.span.start as usize..function.span.end as usize]);
                }
            }
            Statement::VariableDeclaration(declaration) => {
                for declarator in &declaration.declarations {
                    let (Some(name), Some(init)) = (declarator.id.get_identifier_name(), declarator.init.as_ref()) else { continue };
                    match init {
                        Expression::FunctionExpression(_) | Expression::ArrowFunctionExpression(_) | Expression::ClassExpression(_) => {
                            let span = init.span();
                            consider(name.as_str(), &text[span.start as usize..span.end as usize]);
                        }
                        Expression::ObjectExpression(object) => {
                            let mut total = 0usize;
                            let mut hashed = 0usize;
                            for property in &object.properties {
                                let ObjectPropertyKind::ObjectProperty(property) = property else { continue };
                                if property.computed || !matches!(property.key, PropertyKey::StaticIdentifier(_) | PropertyKey::StringLiteral(_)) {
                                    continue;
                                }
                                total += 1;
                                let resolved = match &property.value {
                                    Expression::Identifier(reference) => constants.get(reference.name.as_str()).copied(),
                                    other => string_value(other).and_then(first_class_token),
                                };
                                if resolved.is_some() {
                                    hashed += 1;
                                }
                            }
                            if hashed >= MIN_CLASS_MAP_PROPERTIES && hashed * 10 >= total * 6 {
                                class_maps.push(name.to_string());
                            }
                        }
                        _ => {}
                    }
                }
            }
            _ => {}
        }
    }

    let mut found = Vec::new();
    for (index, bindings) in matches.into_iter().enumerate() {
        match bindings.len() {
            0 => {}
            1 => found.push(Found { binding: bindings.into_iter().next().unwrap(), internal: index }),
            count => log::warn!("internal '{}' matched {} bindings, not exporting", INTERNALS[index].name, count),
        }
    }
    let mut extra: Vec<(String, String)> = Vec::new();
    if let Some(section) = found.iter().find(|f| INTERNALS[f.internal].name == PANEL_SECTION) {
        let position = parsed.program.body.iter().position(|statement| match statement {
            Statement::FunctionDeclaration(function) => function.id.as_ref().is_some_and(|id| id.name.as_str() == section.binding),
            _ => false,
        });
        if let Some(Statement::VariableDeclaration(declaration)) = position.and_then(|index| parsed.program.body.get(index + 1)) {
            if let Some(declarator) = declaration.declarations.first() {
                if let (Some(name), Some(Expression::CallExpression(call))) = (declarator.id.get_identifier_name(), declarator.init.as_ref()) {
                    let is_class_factory = matches!(call.arguments.as_slice(), [argument] if matches!(argument.as_expression(), Some(Expression::Identifier(reference)) if constants.contains_key(reference.name.as_str())));
                    if is_class_factory {
                        extra.push((PANEL_SECTION_ROW.to_string(), name.to_string()));
                    }
                }
            }
        }
    }
    let (wrappers, react) = if wrappers::has_candidate(text) {
        (wrappers::find(&parsed.program, text), wrappers::react_alias(&parsed.program, text))
    } else {
        (Vec::new(), None)
    };
    Ok(Analysis { found, class_maps, extra, wrappers, react })
}

pub fn patch(name: &str, content: &[u8]) -> (Vec<u8>, usize) {
    let Ok(text) = std::str::from_utf8(content) else {
        return (content.to_vec(), 0);
    };
    if !has_candidate(text) {
        return (content.to_vec(), 0);
    }
    let Analysis { found, class_maps, extra, wrappers: wrap_targets, react } = match analyse(text) {
        Ok(analysis) => analysis,
        Err(reason) => {
            log::warn!("{}: not exporting internals or wrapping components, the file does not parse ({})", name, reason);
            return (content.to_vec(), 0);
        }
    };
    let wrap_source = match (&react, wrap_targets.is_empty()) {
        (Some(alias), false) => Some(wrappers::apply(text, &wrap_targets, alias)),
        (None, false) => {
            log::warn!("{}: found {} wrapper target(s) but no React alias, not wrapping", name, wrap_targets.len());
            None
        }
        _ => None,
    };
    let wrapped = if wrap_source.is_some() { wrap_targets.len() } else { 0 };
    if found.is_empty() && class_maps.is_empty() && wrapped == 0 {
        return (content.to_vec(), 0);
    }

    let mut specifiers: Vec<String> = found.iter().map(|f| format!("{} as {}{}", f.binding, INTERNAL_PREFIX, INTERNALS[f.internal].name)).collect();
    specifiers.extend(extra.iter().map(|(name, binding)| format!("{} as {}{}", binding, INTERNAL_PREFIX, name)));
    specifiers.extend(class_maps.iter().enumerate().map(|(index, binding)| format!("{} as {}{}", binding, CLASS_MAP_PREFIX, index)));

    let count = specifiers.len() + wrapped;
    if wrapped > 0 {
        log::info!("{}: wrapping {} component(s) [{}]", name, wrapped, wrap_targets.iter().map(|w| format!("{}={}", w.key, w.binding)).collect::<Vec<_>>().join(", "));
    }
    log::info!(
        "{}: exporting {} internal(s) [{}] and {} class map(s)",
        name,
        found.len() + extra.len(),
        found.iter().map(|f| INTERNALS[f.internal].name).chain(extra.iter().map(|(n, _)| n.as_str())).collect::<Vec<_>>().join(", "),
        class_maps.len()
    );

    let base = wrap_source.as_deref().unwrap_or(text);
    let mut out = Vec::with_capacity(base.len() + 64 + count * 48);
    out.extend_from_slice(base.as_bytes());
    if !specifiers.is_empty() {
        out.extend_from_slice(format!("\nexport{{{}}};\n", specifiers.join(",")).as_bytes());
    }
    (out, count)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn patched(source: &str) -> (String, usize) {
        let (out, count) = patch("test.js", source.as_bytes());
        (String::from_utf8(out).unwrap(), count)
    }

    #[test]
    fn exports_a_matching_function_declaration() {
        let source = "function bL(e){let{nProgress:t,nTransitionSec:n=1,margin:r=`none`}=e;return t}export{bL as unrelated};";
        let (out, count) = patched(source);
        assert_eq!(count, 1);
        assert!(out.ends_with("export{bL as __millennium_internal_ProgressBar};\n"));
        assert!(crate::syntax::check(&out).is_empty());
    }

    #[test]
    fn exports_an_arrow_function_from_a_variable_list() {
        let source = "var a=1,FB=e=>{let{className:t,children:n,size:r=`medium`,background:i}=e;let{strMovieUrl:o}=P();return t};";
        let (out, count) = patched(source);
        assert_eq!(count, 1);
        assert!(out.contains("export{FB as __millennium_internal_SteamSpinner};"));
        assert!(crate::syntax::check(&out).is_empty());
    }

    #[test]
    fn exports_class_maps_built_from_hash_constants() {
        let mut source = String::new();
        for i in 0..24 {
            source.push_str(&format!("var h{}=`{}aaaaaaaaaa-`;", i, (b'a' + (i % 26) as u8) as char));
        }
        source.push_str("var M={Foo:h0,Bar:h1,Baz:h2,Qux:`bbbbbbbbbbb-`};");
        let (out, count) = patched(&source);
        assert_eq!(count, 1);
        assert!(out.contains("export{M as __millennium_classmap_0};"));
        assert!(crate::syntax::check(&out).is_empty());
    }

    #[test]
    fn exports_class_maps_whose_hashes_have_a_leading_underscore() {
        let mut source = String::new();
        for i in 0..24 {
            source.push_str(&format!("var h{}=`{}aaaaaaaaaa-`;", i, (b'a' + (i % 26) as u8) as char));
        }
        source.push_str("var M={Foo:`_5TOP1rhjK9g-`,Bar:`_6e5Lw2oBEA8-`,Baz:`_7IQ7dO6JJsQ-`,Qux:`bbbbbbbbbbb-`};");
        let (out, count) = patched(&source);
        assert_eq!(count, 1);
        assert!(out.contains("export{M as __millennium_classmap_0};"));
    }

    #[test]
    fn exports_panel_section_and_the_row_factory_that_follows_it() {
        let source = "var a=`aaaaaaaaaaa-`,b=`_bbbbbbbbbbb-`,c=`ccccccccccc-`;function pC(e){let{title:t,children:n}=e;return(0,Y.jsxs)(`div`,{className:a,children:[t&&(0,Y.jsxs)(`div`,{className:`_9wgVBRkm6zw-`,children:[(0,Y.jsx)(`div`,{className:`yBEtdT3lf6U-`,children:t}),e.spinner&&(0,Y.jsx)(_a,{})]}),n]})}var mC=oue(c);";
        let (out, count) = patched(source);
        assert_eq!(count, 2);
        assert!(out.contains("pC as __millennium_internal_PanelSection"));
        assert!(out.contains("mC as __millennium_internal_PanelSectionRow"));
        assert!(crate::syntax::check(&out).is_empty());
    }

    #[test]
    fn exports_the_row_when_it_is_the_first_declarator_of_a_longer_var_list() {
        let source = "var a=`aaaaaaaaaaa-`,c=`_ccccccccccc-`;function pC(e){let{title:t,children:n}=e;return(0,Y.jsxs)(`div`,{className:a,children:[t&&(0,Y.jsxs)(`div`,{className:`_9wgVBRkm6zw-`,children:[(0,Y.jsx)(`div`,{className:`yBEtdT3lf6U-`,children:t}),e.spinner&&(0,Y.jsx)(_a,{})]}),n]})}var mC=oue(c),Xgt=e=>e,Zgt=void 0;";
        let (out, count) = patched(source);
        assert_eq!(count, 2);
        assert!(out.contains("mC as __millennium_internal_PanelSectionRow"));
        assert!(crate::syntax::check(&out).is_empty());
    }

    #[test]
    fn does_not_export_a_row_when_the_next_statement_is_not_a_class_factory_call() {
        let source = "var a=`aaaaaaaaaaa-`;function pC(e){let{title:t,children:n}=e;return(0,Y.jsxs)(`div`,{className:a,children:[t&&(0,Y.jsxs)(`div`,{className:`_9wgVBRkm6zw-`,children:[(0,Y.jsx)(`div`,{className:`yBEtdT3lf6U-`,children:t}),e.spinner&&(0,Y.jsx)(_a,{})]}),n]})}var mC=other(1,2);";
        let (out, count) = patched(source);
        assert_eq!(count, 1);
        assert!(!out.contains("PanelSectionRow"));
    }

    #[test]
    fn exports_a_class_expression_bound_to_a_variable() {
        let source = "var zXn=class e extends q.Component{parser;constructor(t){super(t),this.parser=new Gu(e.BBCodeDictionary,e.ElementAccumulator,O.LANGUAGE)}static ElementAccumulator(){return new Xu(new Yu)}static BBCodeDictionary=new Map([]);render(){return this.parser.ParseBBCode(this.props.text,{})}},other=1;";
        let (out, count) = patched(source);
        assert_eq!(count, 1);
        assert!(out.contains("zXn as __millennium_internal_BBCodeParser"));
        assert!(crate::syntax::check(&out).is_empty());
    }

    #[test]
    fn leaves_a_candidate_that_does_not_parse_untouched() {
        let source = "function bL(e){let{nProgress:t,nTransitionSec:n=1,margin:r=`none`}=e;return t}}}";
        let (out, count) = patched(source);
        assert_eq!(count, 0);
        assert_eq!(out, source);
        assert!(analyse(source).is_err());
    }

    #[test]
    fn leaves_unrelated_files_untouched() {
        let source = "function add(a,b){return a+b}export{add};";
        let (out, count) = patched(source);
        assert_eq!(count, 0);
        assert_eq!(out, source);
    }

    #[test]
    fn skips_an_internal_that_matches_more_than_once() {
        let source = "function a(e){return{routePath:e.match?.path}}function b(e){return{routePath:e.match?.path}}";
        let (out, count) = patched(source);
        assert_eq!(count, 0);
        assert_eq!(out, source);
    }

    #[test]
    fn exports_from_a_real_steamui_directory() {
        let Ok(dir) = std::env::var("MILLENNIUM_STEAMUI_DIR") else { return };
        let mut internals: HashMap<String, usize> = HashMap::new();
        let mut class_maps = 0usize;
        let mut files = 0usize;
        let mut invalid = Vec::new();
        let started = std::time::Instant::now();

        for entry in std::fs::read_dir(&dir).unwrap().flatten() {
            let path = entry.path();
            if path.extension().and_then(|e| e.to_str()) != Some("js") {
                continue;
            }
            let content = std::fs::read(&path).unwrap();
            let name = path.file_name().unwrap().to_string_lossy().to_string();
            let before = std::time::Instant::now();
            let (out, count) = patch(&name, &content);
            let elapsed = before.elapsed().as_millis();
            if count == 0 {
                continue;
            }
            files += 1;
            let text = std::str::from_utf8(&out).unwrap();
            if !crate::syntax::check(text).is_empty() {
                invalid.push(name.clone());
            }
            let tail = text.rfind("\nexport{").map(|index| &text[index..]).unwrap_or("");
            for specifier in tail.trim().trim_start_matches("export{").trim_end_matches("};").split(',') {
                if let Some((binding, alias)) = specifier.split_once(" as ") {
                    if let Some(internal) = alias.strip_prefix(INTERNAL_PREFIX) {
                        println!("  {} -> {} ({} ms) in {}", internal, binding, elapsed, name);
                        *internals.entry(internal.to_string()).or_default() += 1;
                    } else if alias.starts_with(CLASS_MAP_PREFIX) {
                        class_maps += 1;
                    }
                }
            }
        }
        println!("files patched: {}, class maps: {}, total {} ms, invalid: {:?}", files, class_maps, started.elapsed().as_millis(), invalid);
        assert!(invalid.is_empty());
        for def in INTERNALS {
            assert_eq!(internals.get(def.name).copied().unwrap_or(0), 1, "{} should be exported exactly once", def.name);
        }
    }

    #[test]
    fn ignores_nested_functions() {
        let source = "function outer(){function bL(e){let{nProgress:t,nTransitionSec:n=1,margin:r=`none`}=e;return t}return bL}";
        let (_, count) = patched(source);
        assert_eq!(count, 0);
    }
}
