use oxc_ast::ast::{Expression, ImportDeclarationSpecifier, Program, Statement};
use oxc_span::GetSpan;
use regex::Regex;
use std::collections::{HashMap, HashSet};
use std::sync::OnceLock;

const ORIGINAL_SUFFIX: &str = "__millennium_original";
const MIN_REACT_HOOK_USES: usize = 10;
const MAX_ROUTER_SOURCE: usize = 20_000;

pub const DESKTOP_ROUTER: &str = "DesktopRouter";
pub const ROOT_MENU_CONTENT: &str = "RootMenuContent";

const ROUTER_ANCHORS: &[&str] = &[".IsMainDesktopWindow()", "bShowTitleBarControls:", "bShowSuperNav:", "bShowDesktopUIContent:"];

pub struct Wrapper {
    pub key: &'static str,
    pub binding: String,
    pub rename: (usize, usize),
}

fn root_menu_pattern() -> &'static Regex {
    static PATTERN: OnceLock<Regex> = OnceLock::new();
    PATTERN.get_or_init(|| {
        Regex::new(r#"title:(?:`Steam`|"Steam"),menuContent:\(0,[\w$]+\.jsxs?\)\(([\w$]+),"#).expect("root menu pattern must compile")
    })
}

fn react_hook_pattern() -> &'static Regex {
    static PATTERN: OnceLock<Regex> = OnceLock::new();
    PATTERN.get_or_init(|| Regex::new(r"([\w$]+)\.useState\(").expect("react hook pattern must compile"))
}

pub fn has_candidate(text: &str) -> bool {
    text.contains("bShowDesktopUIContent:") || text.contains("menuContent:")
}

fn top_level_names<'a>(program: &'a Program<'a>) -> HashSet<&'a str> {
    let mut names = HashSet::new();
    for statement in &program.body {
        match statement {
            Statement::ImportDeclaration(import) => {
                for specifier in import.specifiers.iter().flatten() {
                    let local = match specifier {
                        ImportDeclarationSpecifier::ImportSpecifier(item) => &item.local,
                        ImportDeclarationSpecifier::ImportDefaultSpecifier(item) => &item.local,
                        ImportDeclarationSpecifier::ImportNamespaceSpecifier(item) => &item.local,
                    };
                    names.insert(local.name.as_str());
                }
            }
            Statement::VariableDeclaration(declaration) => {
                for declarator in &declaration.declarations {
                    if let Some(name) = declarator.id.get_identifier_name() {
                        names.insert(name.as_str());
                    }
                }
            }
            Statement::FunctionDeclaration(function) => {
                if let Some(id) = &function.id {
                    names.insert(id.name.as_str());
                }
            }
            _ => {}
        }
    }
    names
}

pub fn react_alias(program: &Program, text: &str) -> Option<String> {
    let mut uses: HashMap<&str, usize> = HashMap::new();
    for capture in react_hook_pattern().captures_iter(text) {
        if let Some(alias) = capture.get(1) {
            *uses.entry(alias.as_str()).or_default() += 1;
        }
    }
    let declared = top_level_names(program);
    uses.into_iter()
        .filter(|(alias, count)| *count >= MIN_REACT_HOOK_USES && declared.contains(alias))
        .max_by_key(|(_, count)| *count)
        .map(|(alias, _)| alias.to_string())
}

fn find_router(program: &Program, text: &str) -> Vec<Wrapper> {
    let mut found = Vec::new();
    for statement in &program.body {
        let Statement::FunctionDeclaration(function) = statement else { continue };
        let Some(id) = &function.id else { continue };
        let start = function.span.start as usize;
        let end = function.span.end as usize;
        if end - start > MAX_ROUTER_SOURCE {
            continue;
        }
        let source = &text[start..end];
        if ROUTER_ANCHORS.iter().all(|anchor| source.contains(anchor)) {
            found.push(Wrapper { key: DESKTOP_ROUTER, binding: id.name.to_string(), rename: (id.span.start as usize, id.span.end as usize) });
        }
    }
    found
}

fn find_declaration(program: &Program, binding: &str) -> Vec<(usize, usize)> {
    let mut spans = Vec::new();
    for statement in &program.body {
        match statement {
            Statement::FunctionDeclaration(function) => {
                if let Some(id) = function.id.as_ref().filter(|id| id.name.as_str() == binding) {
                    spans.push((id.span.start as usize, id.span.end as usize));
                }
            }
            Statement::VariableDeclaration(declaration) => {
                for declarator in &declaration.declarations {
                    let named = declarator.id.get_identifier_name().is_some_and(|name| name.as_str() == binding);
                    let callable = matches!(declarator.init, Some(Expression::ArrowFunctionExpression(_) | Expression::FunctionExpression(_)));
                    if named && callable {
                        let span = declarator.id.span();
                        spans.push((span.start as usize, span.end as usize));
                    }
                }
            }
            _ => {}
        }
    }
    spans
}

fn find_root_menu(program: &Program, text: &str) -> Vec<Wrapper> {
    let bindings: HashSet<&str> = root_menu_pattern().captures_iter(text).filter_map(|capture| capture.get(1)).map(|item| item.as_str()).collect();
    let mut found = Vec::new();
    for binding in bindings {
        let spans = find_declaration(program, binding);
        if spans.len() == 1 {
            found.push(Wrapper { key: ROOT_MENU_CONTENT, binding: binding.to_string(), rename: spans[0] });
        }
    }
    found
}

pub fn find(program: &Program, text: &str) -> Vec<Wrapper> {
    let mut all = Vec::new();
    for (key, candidates) in [(DESKTOP_ROUTER, find_router(program, text)), (ROOT_MENU_CONTENT, find_root_menu(program, text))] {
        match candidates.len() {
            0 => {}
            1 => all.extend(candidates),
            count => log::warn!("wrapper '{}' matched {} bindings, not wrapping", key, count),
        }
    }
    all
}

fn registry_prelude() -> String {
    concat!(
        "globalThis.__millennium_registry??=(()=>{const r={version:0,listeners:new Set(),wrappers:{},available:{}};",
        "r.register=(n,f)=>{if(f)r.wrappers[n]=f;else delete r.wrappers[n];r.version++;r.listeners.forEach(l=>l())};",
        "return r})();",
        "const __millennium_subscribe=l=>(globalThis.__millennium_registry.listeners.add(l),()=>globalThis.__millennium_registry.listeners.delete(l));",
        "const __millennium_snapshot=()=>globalThis.__millennium_registry.version;"
    )
    .to_string()
}

fn wrapper_function(wrapper: &Wrapper, react: &str) -> String {
    format!(
        "function {binding}(...a){{{react}.useSyncExternalStore(__millennium_subscribe,__millennium_snapshot);const w=globalThis.__millennium_registry.wrappers.{key};return w?w({binding}{suffix},...a):{binding}{suffix}(...a)}}",
        binding = wrapper.binding,
        react = react,
        key = wrapper.key,
        suffix = ORIGINAL_SUFFIX
    )
}

pub fn apply(text: &str, wrappers: &[Wrapper], react: &str) -> String {
    let mut renames: Vec<&Wrapper> = wrappers.iter().collect();
    renames.sort_by_key(|wrapper| std::cmp::Reverse(wrapper.rename.0));

    let mut out = String::with_capacity(text.len() + 512 + wrappers.len() * 256);
    let mut cursor = text.len();
    let mut pieces: Vec<&str> = Vec::new();
    for wrapper in &renames {
        pieces.push(&text[wrapper.rename.1..cursor]);
        pieces.push(ORIGINAL_SUFFIX);
        pieces.push(&text[wrapper.rename.0..wrapper.rename.1]);
        cursor = wrapper.rename.0;
    }
    pieces.push(&text[..cursor]);
    for piece in pieces.iter().rev() {
        out.push_str(piece);
    }

    out.push('\n');
    out.push_str(&registry_prelude());
    for wrapper in wrappers {
        out.push_str(&wrapper_function(wrapper, react));
        out.push_str(&format!("globalThis.__millennium_registry.available.{}=true;", wrapper.key));
    }
    out.push('\n');
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use oxc_allocator::Allocator;
    use oxc_parser::Parser;
    use oxc_span::SourceType;

    const ROUTER: &str = "function U4i(e){let t=HN();let n=t.IsMainDesktopWindow();return(0,Y.jsxs)(`div`,{children:[n&&(0,Y.jsx)(H4i,{bShowTitleBarControls:!e.bSmallMode,bShowSuperNav:!e.bSmallMode}),(0,Y.jsx)(W4i,{cm:e.cm,bShowDesktopUIContent:n})]})}";
    const ROOT_MENU: &str = "var ZBi=e=>{return 1},QBi=e=>{let{bOSX:t,...n}=e;return(0,Y.jsx)(i3,{title:`Steam`,menuContent:(0,Y.jsx)(ZBi,{bOSX:t}),...n,children:1})};";

    fn react_source() -> String {
        let uses: String = (0..12).map(|i| format!("q.useState({});", i)).collect();
        format!("var q={{}};{}", uses)
    }

    fn analyse(source: &str) -> (Vec<Wrapper>, Option<String>) {
        let allocator = Allocator::default();
        let parsed = Parser::new(&allocator, source, SourceType::mjs()).parse();
        assert!(parsed.errors.is_empty(), "test source must parse");
        (find(&parsed.program, source), react_alias(&parsed.program, source))
    }

    fn wrapped(source: &str) -> Option<String> {
        let (wrappers, react) = analyse(source);
        if wrappers.is_empty() {
            return None;
        }
        Some(apply(source, &wrappers, &react?))
    }

    fn assert_parses(source: &str) {
        let allocator = Allocator::default();
        let parsed = Parser::new(&allocator, source, SourceType::mjs()).parse();
        assert!(!parsed.panicked && parsed.errors.is_empty(), "output must parse: {:?}\n{}", parsed.errors, source);
    }

    #[test]
    fn finds_the_desktop_router_by_its_property_names() {
        let (wrappers, _) = analyse(&format!("{}{}", react_source(), ROUTER));
        assert_eq!(wrappers.len(), 1);
        assert_eq!(wrappers[0].key, DESKTOP_ROUTER);
        assert_eq!(wrappers[0].binding, "U4i");
    }

    #[test]
    fn finds_the_root_menu_content_component_from_the_steam_menu() {
        let (wrappers, _) = analyse(&format!("{}{}", react_source(), ROOT_MENU));
        assert_eq!(wrappers.len(), 1);
        assert_eq!(wrappers[0].key, ROOT_MENU_CONTENT);
        assert_eq!(wrappers[0].binding, "ZBi");
    }

    #[test]
    fn finds_the_root_menu_when_the_title_uses_double_quotes() {
        let source = ROOT_MENU.replace("`Steam`", "\"Steam\"");
        let (wrappers, _) = analyse(&format!("{}{}", react_source(), source));
        assert_eq!(wrappers.len(), 1);
    }

    #[test]
    fn picks_the_most_used_top_level_react_alias() {
        let (_, react) = analyse(&react_source());
        assert_eq!(react.as_deref(), Some("q"));
    }

    #[test]
    fn ignores_an_alias_that_is_not_a_top_level_binding() {
        let source = (0..12).map(|i| format!("z.useState({});", i)).collect::<String>();
        let (_, react) = analyse(&source);
        assert_eq!(react, None);
    }

    #[test]
    fn renames_the_original_and_appends_a_hoisted_delegating_wrapper() {
        let out = wrapped(&format!("{}{}", react_source(), ROUTER)).unwrap();
        assert!(out.contains("function U4i__millennium_original(e)"));
        assert!(out.contains("function U4i(...a){q.useSyncExternalStore("));
        assert!(out.contains("wrappers.DesktopRouter"));
        assert_parses(&out);
    }

    #[test]
    fn wraps_both_targets_in_one_pass() {
        let out = wrapped(&format!("{}{}{}", react_source(), ROUTER, ROOT_MENU)).unwrap();
        assert!(out.contains("U4i__millennium_original"));
        assert!(out.contains("ZBi__millennium_original=e=>"));
        assert!(out.contains("function ZBi(...a)"));
        assert!(out.contains("(0,Y.jsx)(ZBi,{bOSX:t})"));
        assert_parses(&out);
    }

    #[test]
    fn marks_each_emitted_wrapper_as_available() {
        let out = wrapped(&format!("{}{}{}", react_source(), ROUTER, ROOT_MENU)).unwrap();
        assert!(out.contains("globalThis.__millennium_registry.available.DesktopRouter=true;"));
        assert!(out.contains("globalThis.__millennium_registry.available.RootMenuContent=true;"));
        assert_parses(&out);
    }

    #[test]
    fn defines_the_registry_once_with_a_register_function() {
        let out = wrapped(&format!("{}{}{}", react_source(), ROUTER, ROOT_MENU)).unwrap();
        assert_eq!(out.matches("globalThis.__millennium_registry??=").count(), 1);
        assert!(out.contains("r.register=(n,f)=>"));
    }

    #[test]
    fn does_not_wrap_when_a_router_anchor_is_missing() {
        let source = ROUTER.replace("bShowSuperNav:", "bShowNothing:");
        assert!(analyse(&format!("{}{}", react_source(), source)).0.is_empty());
    }

    #[test]
    fn does_not_wrap_a_root_menu_whose_component_is_not_a_callable_declaration() {
        let source = ROOT_MENU.replace("var ZBi=e=>{return 1},", "var ZBi=5,");
        assert!(analyse(&format!("{}{}", react_source(), source)).0.is_empty());
    }

    #[test]
    fn refuses_to_wrap_ambiguous_routers() {
        let source = format!("{}{}{}", react_source(), ROUTER, ROUTER.replace("U4i", "V5j"));
        assert!(analyse(&source).0.is_empty());
    }

    #[test]
    fn leaves_the_source_alone_without_a_react_alias() {
        assert!(wrapped(ROUTER).is_none());
    }

    #[test]
    fn wraps_the_real_beta_main_chunk() {
        let Ok(dir) = std::env::var("MILLENNIUM_STEAMUI_DIR") else { return };
        let mut wrapped_files = 0;
        for entry in std::fs::read_dir(&dir).unwrap().flatten() {
            let path = entry.path();
            if path.extension().and_then(|e| e.to_str()) != Some("js") {
                continue;
            }
            let text = std::fs::read_to_string(&path).unwrap();
            if !has_candidate(&text) {
                continue;
            }
            let allocator = Allocator::default();
            let parsed = Parser::new(&allocator, &text, SourceType::mjs()).parse();
            if !parsed.errors.is_empty() {
                continue;
            }
            let wrappers = find(&parsed.program, &text);
            if wrappers.is_empty() {
                continue;
            }
            let react = react_alias(&parsed.program, &text).expect("react alias");
            let started = std::time::Instant::now();
            let out = apply(&text, &wrappers, &react);
            println!("{}: react={} wrappers={:?} apply={}ms", path.display(), react, wrappers.iter().map(|w| format!("{}={}", w.key, w.binding)).collect::<Vec<_>>(), started.elapsed().as_millis());
            assert_parses(&out);
            wrapped_files += 1;
        }
        assert_eq!(wrapped_files, 1);
    }
}
