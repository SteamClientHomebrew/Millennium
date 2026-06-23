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
use std::io::{self, Read, Write};
use std::sync::{
    atomic::{AtomicBool, AtomicU64, Ordering},
    mpsc, Arc, Mutex,
};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use serde::Serialize;

#[cfg(unix)]
use std::os::unix::net::UnixStream;
#[cfg(windows)]
use uds_windows::UnixStream;

pub fn default_socket() -> String {
    #[cfg(unix)]
    return "/tmp/millennium-mep.sock".to_owned();

    #[cfg(windows)]
    {
        let tmp = std::env::var("TEMP").unwrap_or_else(|_| r"C:\Windows\Temp".to_owned());
        return format!(r"{}\millennium-mep.sock", tmp);
    }
}

#[derive(Serialize)]
struct Request<'a, P: serde::Serialize> {
    id: &'a str,
    method: &'a str,
    params: P,
}

#[derive(Serialize)]
struct RestartParams<'a> {
    name: &'a str,
    reload_ui: bool,
}

#[derive(Serialize)]
struct StreamParams<'a> {
    name: &'a str,
}

pub struct PrintEntry {
    pub timestamp_us: u64,
    pub source: String,
    pub level: String,
    pub message: String,
    pub file: Option<String>,
    pub line: Option<u32>,
}

pub enum PrintMsg {
    Entry(PrintEntry),
    InitDone,
}

#[derive(Clone)]
pub struct LogColors {
    pub backend: crate::log::Color,
    pub frontend: crate::log::Color,
    pub webview: crate::log::Color,
    pub backend_prefix: Option<String>,
    pub frontend_prefix: Option<String>,
    pub webview_prefix: Option<String>,
}

impl LogColors {
    pub fn from_config(cfg: &crate::config::LoggerConfig) -> Self {
        let rgb = |c: [u8; 3]| crate::log::Color::Rgb(c[0], c[1], c[2]);
        Self {
            backend: rgb(cfg.backend_col),
            frontend: rgb(cfg.frontend_col),
            webview: rgb(cfg.webview_col),
            backend_prefix: cfg.backend_prefix.clone(),
            frontend_prefix: cfg.frontend_prefix.clone(),
            webview_prefix: cfg.webview_prefix.clone(),
        }
    }
}

impl Default for LogColors {
    fn default() -> Self {
        Self::from_config(&crate::config::LoggerConfig::default())
    }
}

fn apply_prefix(template: &str, file: Option<&str>, line: Option<u32>) -> String {
    if !template.contains("{file}") {
        return template.to_owned();
    }
    let loc = match (file, line) {
        (Some(f), Some(n)) if !f.is_empty() && n > 0 => format!("{}:L{}", f, n),
        (Some(f), _) if !f.is_empty() => f.to_owned(),
        _ => String::new(),
    };
    let expanded = template.replace("{file}", &loc);
    // Collapse any whitespace artifacts from empty substitution
    expanded.split_whitespace().collect::<Vec<_>>().join(" ")
}

fn do_print(e: &PrintEntry, colors: &LogColors) {
    use crate::log::{self, Color};

    let ts = log::format_epoch_us(e.timestamp_us);

    let (source_color, prefix_tmpl): (Color, Option<&str>) = match e.source.as_str() {
        "backend" => (colors.backend, colors.backend_prefix.as_deref()),
        "webkit" | "webview" => (colors.webview, colors.webview_prefix.as_deref()),
        _ => (colors.frontend, colors.frontend_prefix.as_deref()),
    };

    let file = e.file.as_deref().filter(|f| !f.is_empty());
    let line = e.line.filter(|&n| n > 0);

    let tag: Option<(String, Color)> = match prefix_tmpl {
        Some(tmpl) => {
            let label = apply_prefix(tmpl, file, line);
            if label.is_empty() { None } else { Some((label, source_color)) }
        }
        None => match (file, line) {
            (Some(f), Some(n)) => Some((format!("{}:L{}", f, n), source_color)),
            _ => match e.source.as_str() {
                "frontend" => Some(("frontend".to_owned(), colors.frontend)),
                "webkit" | "webview" => Some(("webview".to_owned(), colors.webview)),
                _ => None,
            },
        },
    };

    let entry = match tag {
        Some((label, color)) => log::plugin_tag(label, color),
        None => log::plugin_entry(),
    }
    .with_timestamp(ts);

    match e.level.as_str() {
        "error" => entry.error(&e.message),
        "warn" | "warning" => entry.warn(&e.message),
        _ => entry.info(&e.message),
    }
}

pub fn run_printer(rx: mpsc::Receiver<PrintMsg>, colors: Arc<Mutex<LogColors>>) {
    let mut sort_buf: Vec<PrintEntry> = Vec::new();
    let mut init_phase = true;

    loop {
        let first = match rx.recv() {
            Ok(m) => m,
            Err(_) => break,
        };

        match first {
            PrintMsg::Entry(e) => {
                if init_phase {
                    sort_buf.push(e);
                } else {
                    let mut burst = vec![e];
                    let deadline = Instant::now() + Duration::from_millis(15);
                    loop {
                        let remaining = deadline.saturating_duration_since(Instant::now());
                        if remaining.is_zero() {
                            break;
                        }
                        match rx.recv_timeout(remaining) {
                            Ok(PrintMsg::Entry(e2)) => burst.push(e2),
                            Ok(PrintMsg::InitDone) => {}
                            Err(_) => break,
                        }
                    }
                    burst.sort_by_key(|e| e.timestamp_us);
                    let c = colors.lock().unwrap_or_else(|e| e.into_inner()).clone();
                    for e in &burst {
                        do_print(e, &c);
                    }
                }
            }
            PrintMsg::InitDone => {
                if init_phase {
                    init_phase = false;
                    sort_buf.sort_by_key(|e| e.timestamp_us);
                    let c = colors.lock().unwrap_or_else(|e| e.into_inner()).clone();
                    for e in sort_buf.drain(..) {
                        do_print(&e, &c);
                    }
                }
            }
        }
    }
}

fn parse_src_loc(entry: &serde_json::Value) -> (Option<String>, Option<u32>) {
    let file = entry
        .get("file")
        .and_then(|v| v.as_str())
        .filter(|s| !s.is_empty())
        .map(str::to_owned);
    let line = entry
        .get("line")
        .and_then(|v| v.as_u64())
        .map(|n| n as u32)
        .filter(|&n| n > 0);
    (file, line)
}

fn connect(socket: &str) -> anyhow::Result<UnixStream> {
    UnixStream::connect(socket)
        .map_err(|e| anyhow::anyhow!("Cannot connect to Millennium at {}: {}", socket, e))
}

fn write_request<P: serde::Serialize>(
    stream: &mut UnixStream,
    req: &Request<P>,
) -> anyhow::Result<()> {
    let payload = rmp_serde::to_vec_named(req)?;
    let len = payload.len() as u32;
    stream.write_all(&len.to_le_bytes())?;
    stream.write_all(&payload)?;
    Ok(())
}

fn read_frame(stream: &mut UnixStream) -> anyhow::Result<Option<Vec<u8>>> {
    let mut len_buf = [0u8; 4];
    match stream.read_exact(&mut len_buf) {
        Ok(()) => {}
        Err(e) if e.kind() == io::ErrorKind::WouldBlock || e.kind() == io::ErrorKind::TimedOut => {
            return Ok(None);
        }
        Err(e) => return Err(e.into()),
    }
    let len = u32::from_le_bytes(len_buf) as usize;
    let mut body = vec![0u8; len];
    stream.read_exact(&mut body)?;
    Ok(Some(body))
}

fn await_frame(stream: &mut UnixStream, shutdown: &AtomicBool) -> anyhow::Result<Option<Vec<u8>>> {
    loop {
        if shutdown.load(Ordering::Relaxed) {
            return Ok(None);
        }
        match read_frame(stream)? {
            Some(f) => return Ok(Some(f)),
            None => {}
        }
    }
}

pub fn wall_us() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_micros() as u64
}

pub fn stream_crashes(plugin_name: String, socket: String, shutdown: Arc<AtomicBool>) {
    sleep_interruptible(Duration::from_millis(500), &shutdown);
    while !shutdown.load(Ordering::Relaxed) {
        match try_crash_session(&plugin_name, &socket, &shutdown) {
            Ok(()) => {}
            Err(_) => {
                sleep_interruptible(Duration::from_millis(500), &shutdown);
            }
        }
    }
}

fn try_crash_session(plugin_name: &str, socket: &str, shutdown: &AtomicBool) -> anyhow::Result<()> {
    #[derive(serde::Serialize)]
    struct Empty {}

    let req = Request {
        id: "starlight-crash",
        method: "plugin.crash",
        params: Empty {},
    };

    let mut stream = connect(socket)?;
    stream.set_write_timeout(Some(Duration::from_secs(5)))?;
    stream.set_read_timeout(Some(Duration::from_millis(200)))?;
    write_request(&mut stream, &req)?;

    let frame = match await_frame(&mut stream, shutdown)? {
        Some(f) => f,
        None => return Ok(()),
    };
    let init: serde_json::Value = rmp_serde::from_slice(&frame)
        .map_err(|e| anyhow::anyhow!("invalid crash subscribe response: {}", e))?;
    if let Some(err) = init.get("error").and_then(|e| e.as_str()) {
        return Err(anyhow::anyhow!("plugin.crash error: {}", err));
    }

    loop {
        if shutdown.load(Ordering::Relaxed) {
            return Ok(());
        }
        match read_frame(&mut stream)? {
            Some(frame) => {
                if let Ok(val) = rmp_serde::from_slice::<serde_json::Value>(&frame) {
                    if val.get("type").and_then(|t| t.as_str()) == Some("event") {
                        if let Some(data) = val.get("data") {
                            let crashed = data
                                .get("plugin")
                                .and_then(|v| v.as_str())
                                .unwrap_or("");
                            if crashed != plugin_name {
                                continue;
                            }
                            let exit_code = data
                                .get("exit_code")
                                .and_then(|v| v.as_u64())
                                .unwrap_or(0);
                            let msg = if exit_code > 0 && exit_code < 32 {
                                format!(
                                    "Plugin '{}' crashed with signal {}, check crash dump for details",
                                    plugin_name, exit_code
                                )
                            } else if exit_code != 0 {
                                format!(
                                    "Plugin '{}' exited with code 0x{:X}",
                                    plugin_name, exit_code
                                )
                            } else {
                                format!("Plugin '{}' exited unexpectedly", plugin_name)
                            };
                            crate::log::tag("CRASH", crate::log::Color::Red).error(&msg);
                        }
                    }
                }
            }
            None => {}
        }
    }
}

pub fn restart_plugin(plugin_name: &str, socket: &str, reload_steamui: bool) {
    match try_restart(plugin_name, socket, reload_steamui) {
        Ok(()) => crate::log::tag("HMR", crate::log::Color::BabyBlue)
            .info(&format!("Restarted plugin '{}'", plugin_name)),
        Err(_) => {}
    }
}

fn try_restart(plugin_name: &str, socket: &str, reload_steamui: bool) -> anyhow::Result<()> {
    let req = Request {
        id: "starlight-restart",
        method: "plugin.restart",
        params: RestartParams {
            name: plugin_name,
            reload_ui: reload_steamui,
        },
    };

    let mut stream = connect(socket)?;
    stream.set_write_timeout(Some(Duration::from_secs(3)))?;
    stream.set_read_timeout(Some(Duration::from_secs(3)))?;

    write_request(&mut stream, &req)?;
    let _ = read_frame(&mut stream);
    Ok(())
}

pub fn stream_unified(
    plugin_name: String,
    socket: String,
    shutdown: Arc<AtomicBool>,
    reconnect: Arc<AtomicBool>,
    session_start: Arc<AtomicU64>,
    tx: mpsc::Sender<PrintMsg>,
) {
    sleep_interruptible(Duration::from_millis(500), &shutdown);
    let mut last_seen_us: u64 = session_start.load(Ordering::Acquire);
    let mut waiting = false;

    while !shutdown.load(Ordering::Relaxed) {
        match run_unified_session(
            &plugin_name,
            &socket,
            &shutdown,
            &reconnect,
            &mut last_seen_us,
            &tx,
        ) {
            Ok(true) => {
                waiting = false;
                last_seen_us = session_start.load(Ordering::Acquire);
                sleep_interruptible(Duration::from_millis(800), &shutdown);
            }
            Ok(false) => {
                waiting = false;
            }
            Err(_) => {
                if !shutdown.load(Ordering::Relaxed) {
                    if !waiting {
                        waiting = true;
                    }
                    sleep_interruptible(Duration::from_millis(200), &shutdown);
                }
            }
        }
    }
}

fn run_unified_session(
    plugin_name: &str,
    socket: &str,
    shutdown: &AtomicBool,
    reconnect: &AtomicBool,
    last_seen_us: &mut u64,
    tx: &mpsc::Sender<PrintMsg>,
) -> anyhow::Result<bool> {
    let mut stream = connect(socket)?;
    stream.set_write_timeout(Some(Duration::from_secs(5)))?;
    stream.set_read_timeout(Some(Duration::from_millis(200)))?;

    let req = Request {
        id: "starlight-stream",
        method: "plugin.stream",
        params: StreamParams { name: plugin_name },
    };
    write_request(&mut stream, &req)?;

    let frame = match await_frame(&mut stream, shutdown)? {
        Some(f) => f,
        None => return Ok(false),
    };

    let init: serde_json::Value = rmp_serde::from_slice(&frame)
        .map_err(|e| anyhow::anyhow!("invalid subscribe response: {}", e))?;

    if let Some(err) = init.get("error").and_then(|e| e.as_str()) {
        return Err(anyhow::anyhow!("plugin.stream error: {}", err));
    }

    let has_subscription = init
        .pointer("/result/subscription_id")
        .map(|v| !v.is_null())
        .unwrap_or(false);

    if !has_subscription {
        return Err(anyhow::anyhow!(
            "plugin '{}' stream not ready yet",
            plugin_name
        ));
    }

    if let Some(entries) = init.pointer("/result/entries").and_then(|l| l.as_array()) {
        for entry in entries {
            let ts = entry
                .get("timestamp_us")
                .and_then(|v| v.as_u64())
                .unwrap_or(0);
            if ts > *last_seen_us {
                let source = entry
                    .get("source")
                    .and_then(|v| v.as_str())
                    .unwrap_or("frontend")
                    .to_owned();
                let level = entry
                    .get("level")
                    .and_then(|v| v.as_str())
                    .unwrap_or("log")
                    .to_owned();
                let message = entry
                    .get("message")
                    .and_then(|v| v.as_str())
                    .unwrap_or("")
                    .to_owned();
                let (file, line) = parse_src_loc(entry);
                let _ = tx.send(PrintMsg::Entry(PrintEntry {
                    timestamp_us: ts,
                    source,
                    level,
                    message,
                    file,
                    line,
                }));
                *last_seen_us = ts;
            }
        }
    }

    let _ = tx.send(PrintMsg::InitDone);

    loop {
        if shutdown.load(Ordering::Relaxed) {
            return Ok(false);
        }
        if reconnect.swap(false, Ordering::Relaxed) {
            return Ok(true);
        }

        match read_frame(&mut stream)? {
            Some(frame) => {
                if let Ok(val) = rmp_serde::from_slice::<serde_json::Value>(&frame) {
                    if val.get("type").and_then(|t| t.as_str()) == Some("event") {
                        if let Some(data) = val.get("data") {
                            let ts = data
                                .get("timestamp_us")
                                .and_then(|v| v.as_u64())
                                .unwrap_or(0);
                            let source = data
                                .get("source")
                                .and_then(|v| v.as_str())
                                .unwrap_or("frontend")
                                .to_owned();
                            let level = data
                                .get("level")
                                .and_then(|v| v.as_str())
                                .unwrap_or("log")
                                .to_owned();
                            let message = data
                                .get("message")
                                .and_then(|v| v.as_str())
                                .unwrap_or("")
                                .to_owned();
                            let (file, line) = parse_src_loc(data);
                            let _ = tx.send(PrintMsg::Entry(PrintEntry {
                                timestamp_us: ts,
                                source,
                                level,
                                message,
                                file,
                                line,
                            }));
                            if ts > *last_seen_us {
                                *last_seen_us = ts;
                            }
                        }
                    }
                }
            }
            None => {}
        }
    }
}

fn sleep_interruptible(duration: Duration, shutdown: &AtomicBool) {
    let step = Duration::from_millis(50);
    let mut remaining = duration;
    while remaining > Duration::ZERO && !shutdown.load(Ordering::Relaxed) {
        std::thread::sleep(remaining.min(step));
        remaining = remaining.saturating_sub(step);
    }
}
