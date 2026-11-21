#include <catch2/catch_test_macros.hpp>

#include "millennium/theme_class_remapper.h"

#include <filesystem>
#include <fstream>

namespace
{
const std::string k_stable_a = "_3gY0aBuNR8_NPTpXIYfkby";
const std::string k_stable_b = "_1IigUZ3GHaZS2Y-3V3T2rT";
const std::string k_stable_tied = "_4TiedHashAbcdefghijKLm";
const std::string k_stable_unmapped = "_2KhPXikozxMpbIRo_6jPxW";

std::string table_json()
{
    return R"({"build":1,"map":{")" + k_stable_a + R"(":"cGtmpwuGgRg-",")" + k_stable_b +
           R"(":"_9wgVBRkm6zw-"},"multi":{"_4TiedHashAbcdefghijKLm":["TTTTTTTTTTT-","UUUUUUUUUUU-"]},"names":{"Title":["AAAAAAAAAAA-","BBBBBBBBBBB-"],"Solo":["SSSSSSSSSSS-"],"DialogButton":["DDDDDDDDDDD-"],"gpfocus":["GGGGGGGGGGG-"]}})";
}

theme_class_remapper make(std::unordered_set<std::string> live)
{
    return theme_class_remapper([]() { return table_json(); }, [live]() { return live; });
}

std::unordered_set<std::string> beta_live()
{
    return { "cGtmpwuGgRg-", "_9wgVBRkm6zw-", "AAAAAAAAAAA-", "BBBBBBBBBBB-", "SSSSSSSSSSS-", "DDDDDDDDDDD-", "DialogButton" };
}
} // namespace

TEST_CASE("remaps a stable hash selector to its beta class", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    REQUIRE(remapper.active());
    theme_class_remapper::stats counters;
    const auto out = remapper.rewrite("." + k_stable_a + " { color: red; }", &counters);
    CHECK(out == ".cGtmpwuGgRg- { color: red; }");
    CHECK(counters.remapped == 1);
}

TEST_CASE("remaps every class in a compound and descendant selector", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const auto out = remapper.rewrite("." + k_stable_a + "." + k_stable_b + ":not(." + k_stable_a + ") ." + k_stable_b + ":hover{margin:0}");
    CHECK(out == ".cGtmpwuGgRg-._9wgVBRkm6zw-:not(.cGtmpwuGgRg-) ._9wgVBRkm6zw-:hover{margin:0}");
}

TEST_CASE("expands a readable name into every beta class that carried it", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    CHECK(remapper.rewrite(".Title{a:b}") == ":is(.AAAAAAAAAAA-,.BBBBBBBBBBB-){a:b}");
    CHECK(remapper.rewrite(".Solo{a:b}") == ".SSSSSSSSSSS-{a:b}");
}

TEST_CASE("keeps the literal class when beta still defines it", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    CHECK(remapper.rewrite(".DialogButton{a:b}") == ":is(.DialogButton,.DDDDDDDDDDD-){a:b}");
}

TEST_CASE("never expands focus state classes", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    CHECK(remapper.rewrite(".gpfocus{a:b}") == ".gpfocus{a:b}");
}

TEST_CASE("leaves unmapped stable hashes untouched and counts them", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    theme_class_remapper::stats counters;
    const std::string css = "." + k_stable_unmapped + "{a:b}";
    CHECK(remapper.rewrite(css, &counters) == css);
    CHECK(counters.unmapped == 1);
    CHECK(counters.remapped == 0);
}

TEST_CASE("returns css that needs no change byte for byte", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string css = "/* header */\n:root {\n\t--a: 1px !important;\n}\n\n.plain > .other  ,  .more {\n  color:  red ;\n}\n";
    CHECK(remapper.rewrite(css) == css);
}

TEST_CASE("only rewrites inside selectors", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string css = ".x{content:\"." + k_stable_a + "\";background:url(." + k_stable_a + ".png);background:url('." + k_stable_a + "');--v:." + k_stable_a + "}";
    CHECK(remapper.rewrite(css) == css);
}

TEST_CASE("does not touch attribute selectors or comments", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string css = "[class*=\"." + k_stable_a + "\"], /* ." + k_stable_a + " */ [class^='x']{a:b}";
    CHECK(remapper.rewrite(css) == css);
}

TEST_CASE("rewrites rules nested in container at-rules but not their prelude", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const auto out = remapper.rewrite("@media (min-width: 1.5em) { ." + k_stable_a + " { a: b } }");
    CHECK(out == "@media (min-width: 1.5em) { .cGtmpwuGgRg- { a: b } }");
}

TEST_CASE("leaves keyframes and font faces alone", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string css = "@keyframes spin { from { transform: rotate(0deg) } 50% { opacity: .5 } to { transform: rotate(360deg) } }@font-face { font-family: x; src: url(a.woff2) }";
    CHECK(remapper.rewrite(css) == css);
}

TEST_CASE("rewrites nested style rules inside a declaration block", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const auto out = remapper.rewrite("." + k_stable_a + " { color: red; ." + k_stable_b + " { color: blue } &:hover { color: green } }");
    CHECK(out == ".cGtmpwuGgRg- { color: red; ._9wgVBRkm6zw- { color: blue } &:hover { color: green } }");
}

TEST_CASE("treats a custom property block value as opaque", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string css = ".x { --blob: { ." + k_stable_a + " }; color: red }";
    CHECK(remapper.rewrite(css) == css);
}

TEST_CASE("is idempotent", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const auto once = remapper.rewrite("." + k_stable_a + " ." + k_stable_b + ", .Title { a: b }");
    CHECK(remapper.rewrite(once) == once);
}

TEST_CASE("survives malformed css without throwing", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    CHECK_NOTHROW(remapper.rewrite("." + k_stable_a + " { color: red"));
    CHECK_NOTHROW(remapper.rewrite("}}} ." + k_stable_a + " {{{ \" unterminated"));
    CHECK_NOTHROW(remapper.rewrite(""));
    CHECK(remapper.rewrite("").empty());
}

TEST_CASE("reports an active status with the match ratio", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    CHECK(remapper.status().starts_with("active"));
    CHECK(remapper.status().find("2/2") != std::string::npos);
}

TEST_CASE("stays inactive when the mapped classes are not in this steam ui", "[theme_class_remapper]")
{
    auto remapper = make({ "somethingelse" });
    CHECK_FALSE(remapper.active());
    CHECK(remapper.status().starts_with("inactive"));
}

TEST_CASE("rewrite returns the input unchanged when inactive", "[theme_class_remapper]")
{
    auto remapper = make({ "somethingelse" });
    const std::string css = "." + k_stable_a + "{a:b}";
    CHECK(remapper.rewrite(css) == css);
}

TEST_CASE("stays inactive when no table is bundled or it is corrupt", "[theme_class_remapper]")
{
    theme_class_remapper empty([]() { return std::string{}; }, []() { return beta_live(); });
    CHECK_FALSE(empty.active());

    theme_class_remapper corrupt([]() { return std::string("{not json"); }, []() { return beta_live(); });
    CHECK_FALSE(corrupt.active());
}

TEST_CASE("scans class tokens while skipping strings and comments", "[theme_class_remapper]")
{
    const auto found = theme_class_remapper::scan_class_tokens(".a .b-c{content:\".hidden\"} /* .commented */ .d_e:hover");
    CHECK(found.contains("a"));
    CHECK(found.contains("b-c"));
    CHECK(found.contains("d_e"));
    CHECK_FALSE(found.contains("hidden"));
    CHECK_FALSE(found.contains("commented"));
}

TEST_CASE("rewrite_script replaces mapped hashes inside string literals", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    theme_class_remapper::stats counters;
    const std::string script = "document.querySelector('." + k_stable_a + "." + k_stable_b + " ." + k_stable_unmapped + "');";
    const auto out = remapper.rewrite_script(script, &counters);
    CHECK(out == "document.querySelector('.cGtmpwuGgRg-._9wgVBRkm6zw- ." + k_stable_unmapped + "');");
    CHECK(counters.remapped == 2);
    CHECK(counters.unmapped == 1);
}

TEST_CASE("rewrite_script leaves longer identifiers and ordinary code alone", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string script = "const x = 'prefix" + k_stable_a + "'; el.classList.add('plain-name'); // done";
    CHECK(remapper.rewrite_script(script) == script);
}

TEST_CASE("rewrite_script is a no-op when the remapper is inactive", "[theme_class_remapper]")
{
    auto remapper = make({});
    const std::string script = "'." + k_stable_a + "'";
    CHECK(remapper.rewrite_script(script) == script);
}

TEST_CASE("rewrite_script expands readable names inside selector strings", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    theme_class_remapper::stats counters;
    const auto out = remapper.rewrite_script("el.querySelector('.Solo .DialogButton'); a.querySelector(\".Title\");", &counters);
    CHECK(out == "el.querySelector('.SSSSSSSSSSS- :is(.DialogButton,.DDDDDDDDDDD-)'); a.querySelector(\":is(.AAAAAAAAAAA-,.BBBBBBBBBBB-)\");");
    CHECK(counters.expanded == 3);
}

TEST_CASE("rewrite_script expands names chained after a class", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const auto out = remapper.rewrite_script("q('." + k_stable_a + ".Solo')");
    CHECK(out == "q('.cGtmpwuGgRg-.SSSSSSSSSSS-')");
}

TEST_CASE("rewrite_script leaves property access and dotted words in strings alone", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string script = "const v = obj.Solo; const f = 'file.Solo'; const g = 'a/b.Title';";
    CHECK(remapper.rewrite_script(script) == script);
}

TEST_CASE("rewrite_script does not expand names in comments", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string script = "// find '.Solo' here\n/* '.Title' */ run();";
    CHECK(remapper.rewrite_script(script) == script);
}

TEST_CASE("a tied stable hash expands to every candidate", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    theme_class_remapper::stats counters;
    const auto out = remapper.rewrite("." + k_stable_tied + " .x { a: b }", &counters);
    CHECK(out == ":is(.TTTTTTTTTTT-,.UUUUUUUUUUU-) .x { a: b }");
    CHECK(counters.remapped == 1);
    CHECK(counters.unmapped == 0);
}

TEST_CASE("rewrite_script expands a tied hash in a selector string only", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    CHECK(remapper.rewrite_script("q('." + k_stable_tied + "')") == "q(':is(.TTTTTTTTTTT-,.UUUUUUUUUUU-)')");
    const std::string bare = "el.classList.add('" + k_stable_tied + "')";
    CHECK(remapper.rewrite_script(bare) == bare);
}

namespace
{
std::filesystem::path make_steam_ui(const std::string& name, const std::string& index_html)
{
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "css");
    std::ofstream(root / "index.html") << index_html;
    std::ofstream(root / "css" / "a.css") << ".cGtmpwuGgRg- { color: red; }";
    return root;
}
} // namespace

TEST_CASE("scan_steam_ui reads the classes of a module based UI", "[theme_class_remapper]")
{
    const auto root = make_steam_ui("millennium_remap_module_ui", "<script type=\"module\" src=\"/library.js\"></script>");
    CHECK(theme_class_remapper::scan_steam_ui(root).contains("cGtmpwuGgRg-"));
    std::filesystem::remove_all(root);
}

TEST_CASE("scan_steam_ui ignores leftover module files in a webpack UI", "[theme_class_remapper]")
{
    const auto root = make_steam_ui("millennium_remap_webpack_ui", "<script defer=\"defer\" src=\"/library.js\"></script>");
    CHECK(theme_class_remapper::scan_steam_ui(root).empty());
    auto remapper = theme_class_remapper([]() { return table_json(); }, [root]() { return theme_class_remapper::scan_steam_ui(root); });
    CHECK_FALSE(remapper.active());
    std::filesystem::remove_all(root);
}

TEST_CASE("scan_steam_ui is empty without an index", "[theme_class_remapper]")
{
    CHECK(theme_class_remapper::scan_steam_ui("/nonexistent/steamui").empty());
}

TEST_CASE("rewrite_script leaves member access inside template expressions alone", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string script = "const s = `div${CssClassNames.UserPanel.Solo} > div`; run(`${a.Solo}`);";
    CHECK(remapper.rewrite_script(script) == script);
}

TEST_CASE("rewrite_script still expands selectors in the text parts of a template", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const auto out = remapper.rewrite_script("q(`.Solo ${x.Title} .Solo`)");
    CHECK(out == "q(`.SSSSSSSSSSS- ${x.Title} .SSSSSSSSSSS-`)");
}

TEST_CASE("rewrite_script handles nested templates and strings inside expressions", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const std::string script = "const s = `a ${ f(`b ${ g('.Solo') } c`, {k: 1}) } .Solo`;";
    const auto out = remapper.rewrite_script(script);
    CHECK(out == "const s = `a ${ f(`b ${ g('.SSSSSSSSSSS-') } c`, {k: 1}) } .SSSSSSSSSSS-`;");
}

TEST_CASE("rewrite_script replaces mapped hashes inside template expressions", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    const auto out = remapper.rewrite_script("const s = `${'." + k_stable_a + "'}`;");
    CHECK(out == "const s = `${'.cGtmpwuGgRg-'}`;");
}

TEST_CASE("a letter-only stable hash is recognised and remapped in scripts", "[theme_class_remapper]")
{
    const std::string letters = "NEMXhMlqXOCJwfgWlHXhT";
    auto remapper = theme_class_remapper([letters]() { return std::string(R"({"build":1,"map":{")") + letters + R"(":"jxUdB556VLE-"},"names":{}})"; }, []() { return std::unordered_set<std::string>{ "jxUdB556VLE-" }; });
    REQUIRE(remapper.active());
    CHECK(remapper.rewrite("." + letters + " { a: b }") == ".jxUdB556VLE- { a: b }");
    CHECK(remapper.rewrite_script("q('." + letters + "')") == "q('.jxUdB556VLE-')");
}

TEST_CASE("readable camel case names are not mistaken for letter-only hashes", "[theme_class_remapper]")
{
    auto remapper = make(beta_live());
    theme_class_remapper::stats counters;
    remapper.rewrite(".BroadcastDetailsSection { a: b }", &counters);
    CHECK(counters.unmapped == 0);
}
