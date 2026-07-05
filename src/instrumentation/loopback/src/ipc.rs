use serde_json::Value;
use std::fs::File;
use std::io::{Read, Write};
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

#[cfg(not(windows))]
use std::os::unix::io::FromRawFd;
#[cfg(windows)]
use std::os::windows::io::FromRawHandle;

pub struct PatchDef {
    pub plugin: String,
    pub file: String,
    pub find: String,
    pub transforms: Vec<(String, String)>,
}

static WRITE_CH: OnceLock<Mutex<File>> = OnceLock::new();

fn parse_args() -> Option<(File, File)> {
    #[cfg(not(windows))]
    {
        for arg in std::env::args() {
            if let Some(val) = arg.strip_prefix("--millennium-loopback-ipc-fds=") {
                let mut parts = val.splitn(2, ',');
                let r: i32 = parts.next()?.parse().ok()?;
                let w: i32 = parts.next()?.parse().ok()?;
                unsafe {
                    return Some((File::from_raw_fd(r), File::from_raw_fd(w)));
                }
            }
        }
        None
    }
    #[cfg(windows)]
    {
        for arg in std::env::args() {
            if let Some(val) = arg.strip_prefix("--millennium-loopback-ipc-handles=") {
                let mut parts = val.splitn(2, ',');
                let r: usize = parts.next()?.parse().ok()?;
                let w: usize = parts.next()?.parse().ok()?;
                unsafe {
                    return Some((
                        File::from_raw_handle(r as *mut std::ffi::c_void),
                        File::from_raw_handle(w as *mut std::ffi::c_void),
                    ));
                }
            }
        }
        None
    }
}

fn write_frame(file: &mut File, payload: &[u8]) -> std::io::Result<()> {
    let len = (payload.len() as u32).to_le_bytes();
    file.write_all(&len)?;
    file.write_all(payload)
}

fn read_frame(file: &mut File) -> std::io::Result<Vec<u8>> {
    let mut len_buf = [0u8; 4];
    file.read_exact(&mut len_buf)?;
    let len = u32::from_le_bytes(len_buf) as usize;
    if len == 0 || len > 8 * 1024 * 1024 {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "bad frame length",
        ));
    }
    let mut body = vec![0u8; len];
    file.read_exact(&mut body)?;
    Ok(body)
}

fn parse_patch_list(patches_val: &Value) -> Vec<PatchDef> {
    let arr = match patches_val.as_array() {
        Some(a) => a,
        None => return vec![],
    };
    let mut defs = Vec::new();
    for item in arr {
        let plugin = item["plugin"].as_str().unwrap_or("").to_owned();
        let file = item["file"].as_str().unwrap_or("").to_owned();
        let find = item["find"].as_str().unwrap_or("").to_owned();
        let mut transforms = Vec::new();
        if let Some(ts) = item["transforms"].as_array() {
            for t in ts {
                let m = t["match"].as_str().unwrap_or("").to_owned();
                let r = t["replace"].as_str().unwrap_or("").to_owned();
                transforms.push((m, r));
            }
        }
        if !file.is_empty() && !find.is_empty() {
            defs.push(PatchDef {
                plugin,
                file,
                find,
                transforms,
            });
        }
    }
    defs
}

pub fn init<F: Fn(Vec<PatchDef>) + Send + 'static>(on_patch_list: F) {
    let (mut read_file, write_file) = match parse_args() {
        Some(pair) => pair,
        None => {
            log::warn!("no --millennium-loopback-ipc-fds/handles arg; patch engine disabled");
            return;
        }
    };

    WRITE_CH.set(Mutex::new(write_file)).ok();

    std::thread::spawn(move || loop {
        match read_frame(&mut read_file) {
            Ok(frame) => {
                let val = match rmp_serde::from_slice::<Value>(&frame) {
                    Ok(v) => v,
                    Err(_) => continue,
                };
                if val.get("type").and_then(|t| t.as_str()) == Some("patch_list") {
                    let defs = val.get("patches").map(parse_patch_list).unwrap_or_default();
                    on_patch_list(defs);
                }
            }
            Err(_) => break,
        }
    });
}

fn send_frame(payload: &[u8]) {
    if let Some(mu) = WRITE_CH.get() {
        if let Ok(mut file) = mu.lock() {
            let _ = write_frame(&mut *file, payload);
        }
    }
}

pub fn send_log(level: &str, message: &str) {
    let msg = serde_json::json!({"type": "log", "level": level, "message": message});
    if let Ok(payload) = rmp_serde::to_vec_named(&msg) {
        send_frame(&payload);
    }
}

pub fn send_patch_event(
    plugin_name: &str,
    other_plugin: &str,
    filename: &str,
    event_type: &str,
    detail: &str,
    find_pattern: &str,
    transform_patterns: &str,
    before_text: &str,
    after_text: &str,
) {
    let ts = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_micros() as u64;

    let msg = serde_json::json!({
        "type":                "patch_event",
        "event_type":          event_type,
        "plugin":              plugin_name,
        "other_plugin":        other_plugin,
        "filename":            filename,
        "detail":              detail,
        "find_pattern":        find_pattern,
        "transform_patterns":  transform_patterns,
        "before_text":         before_text,
        "after_text":          after_text,
        "timestamp_us":        ts,
    });
    if let Ok(payload) = rmp_serde::to_vec_named(&msg) {
        send_frame(&payload);
    }
}
