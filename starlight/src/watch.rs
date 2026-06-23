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
use crate::{config::DevRuntime, BuildMode};
use notify::{recommended_watcher, RecursiveMode, Watcher};
use std::path::{Path, PathBuf};
use std::sync::{
    atomic::{AtomicBool, AtomicU64, Ordering},
    mpsc, Arc,
};
use std::time::{Duration, Instant};

pub fn watch(config_path: &Path, out_path: Option<&Path>, mode: BuildMode) -> anyhow::Result<()> {
    let config_path = config_path
        .canonicalize()
        .map_err(|_| anyhow::anyhow!("config not found: {}", config_path.display()))?;

    let config_dir = config_path.parent().unwrap().to_path_buf();
    let out_owned = out_path.map(|p| p.to_path_buf());
    let dev = run_pack(&config_path, out_owned.as_deref(), mode)
        .ok()
        .flatten();

    let session_start = Arc::new(AtomicU64::new(crate::mep::wall_us()));
    if let Some(ref d) = dev {
        crate::mep::restart_plugin(&d.plugin_name, &d.socket, d.reload_steamui_when.is_active());
    }

    let shutdown = Arc::new(AtomicBool::new(false));
    let reconnect = Arc::new(AtomicBool::new(false));

    let (tx, rx) = std::sync::mpsc::channel::<crate::mep::PrintMsg>();

    let printer_thread = std::thread::spawn(move || {
        crate::mep::run_printer(rx);
    });

    let stream_thread = dev.as_ref().map(|d| {
        let plugin_name = d.plugin_name.clone();
        let socket = d.socket.clone();
        let shutdown = Arc::clone(&shutdown);
        let reconnect = Arc::clone(&reconnect);
        let session_start = Arc::clone(&session_start);
        let tx = tx.clone();
        std::thread::spawn(move || {
            crate::mep::stream_unified(plugin_name, socket, shutdown, reconnect, session_start, tx);
        })
    });

    drop(tx);

    let (fs_tx, fs_rx) = mpsc::channel::<notify::Result<notify::Event>>();
    let mut watcher = recommended_watcher(move |res| {
        let _ = fs_tx.send(res);
    })
    .map_err(|e| anyhow::anyhow!("failed to create watcher: {}", e))?;

    watcher
        .watch(&config_path, RecursiveMode::NonRecursive)
        .map_err(|e| anyhow::anyhow!("watch error: {}", e))?;
    watcher
        .watch(&config_dir, RecursiveMode::Recursive)
        .map_err(|e| anyhow::anyhow!("watch error: {}", e))?;

    crate::log::info("Press CTRL^C to exit...");

    loop {
        let mut changed: Vec<PathBuf> = match fs_rx.recv() {
            Err(_) => break,
            Ok(Err(e)) => {
                crate::log::error(&format!("watch error: {}", e));
                continue;
            }
            Ok(Ok(event)) => event.paths,
        };

        let deadline = Instant::now() + Duration::from_millis(100);
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                break;
            }
            match fs_rx.recv_timeout(remaining) {
                Ok(Ok(event)) => changed.extend(event.paths),
                Ok(Err(_)) => {}
                Err(mpsc::RecvTimeoutError::Timeout) => break,
                Err(mpsc::RecvTimeoutError::Disconnected) => break,
            }
        }

        crate::log::info("change detected");
        match run_pack(&config_path, out_owned.as_deref(), mode) {
            Err(()) => {}
            Ok(new_dev) => {
                let effective_dev = new_dev.as_ref().or(dev.as_ref());
                if let Some(d) = effective_dev {
                    if d.auto_restart {
                        let reload_ui = changed.iter().any(|p| d.reload_steamui_when.matches(p));
                        session_start.store(crate::mep::wall_us(), Ordering::Release);
                        crate::mep::restart_plugin(&d.plugin_name, &d.socket, reload_ui);
                        reconnect.store(true, Ordering::Relaxed);
                    }
                }
            }
        }
    }

    stop(shutdown, stream_thread, printer_thread)
}

fn stop(
    shutdown: Arc<AtomicBool>,
    stream_thread: Option<std::thread::JoinHandle<()>>,
    printer_thread: std::thread::JoinHandle<()>,
) -> anyhow::Result<()> {
    shutdown.store(true, Ordering::Relaxed);
    if let Some(t) = stream_thread {
        let _ = t.join();
    }
    let _ = printer_thread.join();
    Ok(())
}

fn run_pack(
    config_path: &Path,
    out_path: Option<&Path>,
    mode: BuildMode,
) -> Result<Option<DevRuntime>, ()> {
    match crate::pack::pack(config_path, out_path, mode) {
        Ok(dev) => Ok(dev),
        Err(e) => {
            if e.downcast_ref::<crate::bundler::js::BundleCompileError>()
                .is_none()
            {
                crate::print_error(&e);
            }
            Err(())
        }
    }
}
