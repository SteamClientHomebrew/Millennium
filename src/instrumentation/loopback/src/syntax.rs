use oxc_allocator::Allocator;
use oxc_parser::Parser;
use oxc_span::SourceType;

pub fn check(source: &str) -> Vec<String> {
    let alloc = Allocator::default();
    let ret = Parser::new(&alloc, source, SourceType::mjs()).parse();
    ret.errors
        .iter()
        .map(|err| render_error(source, err))
        .collect()
}

fn render_error(source: &str, err: &oxc_diagnostics::OxcDiagnostic) -> String {
    let message = err.message.to_string();

    let span = err
        .labels
        .as_ref()
        .and_then(|ls| ls.first())
        .map(|l| (l.offset(), l.len().max(1)));

    let Some((raw_offset, len)) = span else {
        return message;
    };

    let offset = raw_offset.min(source.len());
    let offset = (0..=offset)
        .rev()
        .find(|&i| source.is_char_boundary(i))
        .unwrap_or(0);

    let before = &source[..offset];
    let line_no = before.bytes().filter(|&b| b == b'\n').count() + 1;
    let line_start = before.rfind('\n').map(|i| i + 1).unwrap_or(0);
    let col = offset - line_start;

    let line_end = source[offset..]
        .find('\n')
        .map(|i| offset + i)
        .unwrap_or(source.len());
    let full_line = &source[line_start..line_end];

    /* preview the error */
    const CTX: usize = 60;
    let (snippet, caret_col) = if full_line.len() > CTX * 2 {
        let start = col.saturating_sub(CTX);
        let start = (0..=start)
            .rev()
            .find(|&i| full_line.is_char_boundary(i))
            .unwrap_or(0);
        let end = (start + CTX * 2).min(full_line.len());
        let end = (end..=full_line.len())
            .find(|&i| full_line.is_char_boundary(i))
            .unwrap_or(full_line.len());
        (&full_line[start..end], col - start)
    } else {
        (full_line, col)
    };

    let underline_len = len.min(snippet.len().saturating_sub(caret_col)).max(1);
    let underline = format!("{}{}", " ".repeat(caret_col), "^".repeat(underline_len));

    format!("{message}\n  {line_no} | {snippet}\n    | {underline}")
}
