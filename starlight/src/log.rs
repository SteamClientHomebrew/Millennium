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
use owo_colors::OwoColorize;
use std::sync::{
    atomic::{AtomicI32, Ordering},
    Mutex, OnceLock,
};
use std::time::SystemTime;

static STDOUT_LOCK: OnceLock<Mutex<()>> = OnceLock::new();
static GLOBAL_TAG_PADDING: AtomicI32 = AtomicI32::new(-15);

pub fn set_global_tag_padding(v: i32) {
    GLOBAL_TAG_PADDING.store(v, Ordering::Relaxed);
}

fn is_tty() -> bool {
    static TTY: OnceLock<bool> = OnceLock::new();
    use std::io::IsTerminal;
    *TTY.get_or_init(|| std::io::stdout().is_terminal())
}

fn colors_enabled() -> bool {
    static ENABLED: OnceLock<bool> = OnceLock::new();
    *ENABLED.get_or_init(|| is_tty() && std::env::var_os("NO_COLOR").map_or(true, |v| v.is_empty()))
}

pub fn green(s: &str) -> String {
    if colors_enabled() {
        s.bright_green().to_string()
    } else {
        s.to_owned()
    }
}

pub fn dim(s: &str) -> String {
    if colors_enabled() {
        s.dimmed().to_string()
    } else {
        s.to_owned()
    }
}

pub fn orange(s: &str) -> String {
    if colors_enabled() {
        s.truecolor(255, 165, 0).to_string()
    } else {
        s.to_owned()
    }
}

pub fn dim_orange(s: &str) -> String {
    if colors_enabled() {
        s.truecolor(230, 140, 30).to_string()
    } else {
        s.to_owned()
    }
}

#[allow(dead_code)]
#[derive(Clone, Copy)]
pub enum Color {
    Red,
    Yellow,
    Orange,
    BrightGreen,
    BabyBlue,
    Blue,
    Magenta,
    Cyan,
    Rgb(u8, u8, u8),
}

impl Color {
    fn apply(self, s: &str) -> String {
        if !colors_enabled() {
            return s.to_owned();
        }
        match self {
            Color::Red => s.red().to_string(),
            Color::Yellow => s.yellow().to_string(),
            Color::Orange => orange(s),
            Color::BrightGreen => s.bright_green().to_string(),
            Color::BabyBlue => s.truecolor(100, 210, 255).to_string(),
            Color::Blue => s.blue().to_string(),
            Color::Magenta => s.magenta().to_string(),
            Color::Cyan => s.cyan().to_string(),
            Color::Rgb(r, g, b) => s.truecolor(r, g, b).to_string(),
        }
    }
}

enum Level {
    Info,
    Warn,
    Error,
}

impl Level {
    fn style_time(&self, s: &str) -> String {
        if !colors_enabled() {
            return s.to_owned();
        }
        match self {
            Level::Info => s.dimmed().to_string(),
            Level::Warn => dim_orange(s),
            Level::Error => s.truecolor(255, 80, 80).to_string(),
        }
    }

    fn style_message(&self, s: &str) -> String {
        if !colors_enabled() {
            return s.to_owned();
        }
        match self {
            Level::Info => s.to_owned(),
            Level::Warn => s.to_owned(),
            Level::Error => s.truecolor(255, 80, 80).to_string(),
        }
    }
}

#[must_use]
pub struct LogEntry {
    tag: Option<(String, Color)>,
    timestamp: Option<String>,
    overwrite: bool,
}

pub fn entry() -> LogEntry {
    LogEntry { tag: None, timestamp: None, overwrite: false }
}

pub fn tag(label: impl Into<String>, color: Color) -> LogEntry {
    LogEntry { tag: Some((label.into(), color)), timestamp: None, overwrite: false }
}

pub fn plugin_entry() -> LogEntry {
    LogEntry { tag: None, timestamp: None, overwrite: false }
}

pub fn plugin_tag(label: impl Into<String>, color: Color) -> LogEntry {
    LogEntry { tag: Some((label.into(), color)), timestamp: None, overwrite: false }
}

impl LogEntry {
    #[allow(dead_code)]
    pub fn overwrite(mut self) -> Self {
        self.overwrite = true;
        self
    }

    pub(crate) fn with_timestamp(mut self, ts: String) -> Self {
        self.timestamp = Some(ts);
        self
    }

    pub fn info(self, message: &str) {
        self.print(Level::Info, message)
    }
    pub fn warn(self, message: &str) {
        self.print(Level::Warn, message)
    }
    pub fn error(self, message: &str) {
        self.print(Level::Error, message)
    }

    fn print(self, level: Level, message: &str) {
        let _guard = STDOUT_LOCK
            .get_or_init(|| Mutex::new(()))
            .lock()
            .unwrap_or_else(|e| e.into_inner());

        let time = self.timestamp.unwrap_or_else(now_hms_us);

        if self.overwrite && is_tty() {
            print!("\x1b[1A\r\x1b[K");
        }

        let pad_setting = GLOBAL_TAG_PADDING.load(Ordering::Relaxed);
        let col_w = pad_setting.unsigned_abs() as usize;
        let time_str = level.style_time(&time);

        let mut lines = message.split('\n');
        let first = lines.next().unwrap_or("");

        match &self.tag {
            Some((label, color)) => {
                let visible_w = label.len().max(col_w);
                let overflow = visible_w - label.len();
                let (pre, post) = if pad_setting > 0 {
                    (String::new(), " ".repeat(overflow))   // left-align: pad right
                } else {
                    (" ".repeat(overflow), String::new())   // right-align: pad left
                };
                let pad = " ".repeat(visible_w + 3 + time.len() + 3);
                println!("{}{}{} › {} › {}", pre, color.apply(label), post, time_str, level.style_message(first));
                for line in lines {
                    println!("{}{}", pad, level.style_message(line));
                }
            }
            None => {
                let pad = " ".repeat(col_w + if col_w > 0 { 3 } else { 0 } + time.len() + 3);
                if col_w > 0 {
                    println!("{} › {} › {}", " ".repeat(col_w), time_str, level.style_message(first));
                } else {
                    println!("{} › {}", time_str, level.style_message(first));
                }
                for line in lines {
                    println!("{}{}", pad, level.style_message(line));
                }
            }
        }
    }
}

pub fn info(message: &str) {
    entry().info(message)
}
pub fn warn(message: &str) {
    entry().warn(message)
}
pub fn error(message: &str) {
    entry().error(message)
}

pub(crate) fn format_epoch_us(us: u64) -> String {
    let s = us / 1_000_000;
    let sub_us = (us % 1_000_000) as u32;
    format!(
        "{:02}:{:02}:{:02}.{:06}",
        (s / 3600) % 24,
        (s / 60) % 60,
        s % 60,
        sub_us
    )
}

fn now_hms_us() -> String {
    let d = SystemTime::now()
        .duration_since(SystemTime::UNIX_EPOCH)
        .unwrap_or_default();
    format_epoch_us(d.as_micros() as u64)
}

pub(crate) fn build_error(header: &str, body: &str) {
    let _guard = STDOUT_LOCK
        .get_or_init(|| Mutex::new(()))
        .lock()
        .unwrap_or_else(|e| e.into_inner());
    const TAG_THRESHOLD: usize = 15;
    let time = now_hms_us();
    println!(
        "{} › {} › {}",
        " ".repeat(TAG_THRESHOLD),
        Level::Error.style_time(&time),
        Level::Error.style_message(header)
    );
    print!("{}", body);
    if !body.ends_with('\n') {
        println!();
    }
}
