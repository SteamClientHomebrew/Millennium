mod engine;
mod ipc;
mod logger;
mod syntax;
mod url;

use engine::{apply_patches, PatchSet};
use std::path::Path;
use std::sync::{Arc, Condvar, Mutex, OnceLock, RwLock};

struct PatchEngine {
    patch_set: Arc<RwLock<Option<PatchSet>>>,
    ready: Arc<(Mutex<bool>, Condvar)>,
}

impl PatchEngine {
    fn new() -> Arc<Self> {
        let engine = Arc::new(PatchEngine {
            patch_set: Arc::new(RwLock::new(None)),
            ready: Arc::new((Mutex::new(false), Condvar::new())),
        });

        let patch_set = Arc::clone(&engine.patch_set);
        let ready = Arc::clone(&engine.ready);
        ipc::init(move |defs| {
            let mut plugins: Vec<&str> = defs.iter().map(|d| d.plugin.as_str()).collect();
            plugins.sort_unstable();
            plugins.dedup();

            log::info!(
                "patch list loaded: {} patch(es) from {} plugin(s): [{}]",
                defs.len(),
                plugins.len(),
                plugins.join(", ")
            );

            let new_set = PatchSet::build(defs);
            if let Ok(mut w) = patch_set.write() {
                *w = new_set;
            }

            let (lock, cvar) = &*ready;
            *lock.lock().unwrap() = true;
            cvar.notify_all();
        });

        engine
    }
}

static ENGINE: OnceLock<Arc<PatchEngine>> = OnceLock::new();

fn get_engine() -> &'static Arc<PatchEngine> {
    ENGINE.get_or_init(|| {
        logger::init();
        PatchEngine::new()
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lb_url_has_local_file(url: *const libc::c_char) -> libc::c_int {
    if url.is_null() {
        return 0;
    }
    let s = unsafe { std::ffi::CStr::from_ptr(url).to_str().unwrap_or("") };
    match url::local_path_from_lb_url(s) {
        Some(p) if p.exists() => 1,
        _ => 0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lb_url_is_patchable(url: *const libc::c_char) -> libc::c_int {
    if url.is_null() {
        return 0;
    }
    let s = unsafe { std::ffi::CStr::from_ptr(url).to_str().unwrap_or("") };
    if url::is_steamloopback_patchable(s) {
        1
    } else {
        0
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lb_handle_request(
    url: *const libc::c_char,
    out_data: *mut *mut u8,
    out_size: *mut u32,
    out_mime_type: *mut *const libc::c_char,
) -> libc::c_int {
    if url.is_null() || out_data.is_null() || out_size.is_null() {
        return -1;
    }

    let url_str = unsafe { std::ffi::CStr::from_ptr(url).to_str().unwrap_or("") };

    if !out_mime_type.is_null() {
        unsafe {
            *out_mime_type = url::mime_type_for_url(url_str).as_ptr();
        }
    }

    let local_path = match url::local_path_from_lb_url(url_str) {
        Some(p) => p,
        None => {
            log::error!(
                "could not resolve steamloopback URL to a local path: {}",
                url_str
            );
            return -1;
        }
    };

    let content = match std::fs::read(&local_path) {
        Ok(c) => c,
        Err(e) => {
            log::error!("failed to read '{}': {}", local_path.display(), e);
            return -1;
        }
    };

    let path_str = local_path.to_str().unwrap_or(url_str);
    let name = Path::new(path_str)
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or(path_str);

    let t_total = std::time::Instant::now();
    let size_kb = content.len() as f64 / 1024.0;

    let final_content = {
        let engine = get_engine();

        {
            let (lock, cvar) = &*engine.ready;
            let ready = lock.lock().unwrap();
            if !*ready {
                let _ = cvar.wait_timeout(ready, std::time::Duration::from_millis(500));
            }
        }

        let guard = match engine.patch_set.read() {
            Ok(g) => g,
            Err(_) => return -1,
        };

        if let Some(patch_set) = guard.as_ref() {
            let t0 = std::time::Instant::now();
            let result = apply_patches(patch_set, &content, path_str);
            let elapsed = t0.elapsed();

            match result {
                None => {
                    log::info!(
                        "{}: no patches matched ({:.1} KB, {:.2}ms)",
                        name,
                        size_kb,
                        elapsed.as_secs_f64() * 1000.0
                    );
                    content
                }
                Some((patched, blamed)) => {
                    log::info!("{}: intercepted ({:.1} KB)", name, size_kb);
                    let t1 = std::time::Instant::now();
                    let errors = if url::is_js_url(url_str) {
                        syntax::check(std::str::from_utf8(&patched).unwrap_or(""))
                    } else {
                        vec![]
                    };
                    let syntax_ms = t1.elapsed().as_secs_f64() * 1000.0;

                    if !errors.is_empty() {
                        let blamed_str = blamed.join(", ");
                        log::warn!(
                            "{}: syntax error after patching (blamed: [{}]) — reverting to original (patch {:.2}ms, syntax {:.2}ms, total {:.2}ms)",
                            name,
                            blamed_str,
                            elapsed.as_secs_f64() * 1000.0,
                            syntax_ms,
                            t_total.elapsed().as_secs_f64() * 1000.0,
                        );
                        for (i, err) in errors.iter().enumerate() {
                            for line in err.lines() {
                                log::warn!("  [{}] #{}: {}", name, i + 1, line);
                            }
                        }
                        let detail = errors.join("|||");
                        ipc::send_patch_event(
                            &blamed_str,
                            "",
                            path_str,
                            "syntax_error",
                            &detail,
                            "",
                            "",
                            "",
                            "",
                        );
                        content
                    } else {
                        let original_kb = content.len() as f64 / 1024.0;
                        let patched_kb = patched.len() as f64 / 1024.0;
                        log::info!(
                            "{}: served patched ({:.1} KB → {:.1} KB, applied by [{}], patch {:.2}ms, syntax {:.2}ms, total {:.2}ms)",
                            name,
                            original_kb,
                            patched_kb,
                            blamed.join(", "),
                            elapsed.as_secs_f64() * 1000.0,
                            syntax_ms,
                            t_total.elapsed().as_secs_f64() * 1000.0,
                        );
                        patched
                    }
                }
            }
        } else {
            log::warn!(
                "{}: patch list not yet loaded ({:.1} KB) — serving original",
                name,
                size_kb
            );
            content
        }
    };

    let len = final_content.len();
    let ptr = unsafe { libc::malloc(len) as *mut u8 };
    if ptr.is_null() {
        return -1;
    }
    unsafe {
        std::ptr::copy_nonoverlapping(final_content.as_ptr(), ptr, len);
        *out_data = ptr;
        *out_size = len as u32;
    }
    0
}
