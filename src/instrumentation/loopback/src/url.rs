use std::path::PathBuf;

fn path_after_host(url: &str) -> Option<&str> {
    let rest = url
        .strip_prefix("http://")
        .or_else(|| url.strip_prefix("https://"))?;
    let rest = rest.strip_prefix("steamloopback.host")?;
    if !rest.is_empty() && !rest.starts_with('/') {
        return None;
    }
    Some(rest)
}

fn ext_before_query(path: &str) -> &str {
    let q = path.find('?').unwrap_or(path.len());
    let before = &path[..q];
    let dot = before.rfind('.').unwrap_or(before.len());
    &before[dot..]
}

pub fn is_steamloopback_patchable(url: &str) -> bool {
    path_after_host(url).is_some()
}

pub fn is_js_url(url: &str) -> bool {
    matches!(
        path_after_host(url).map(ext_before_query),
        Some(".js") | Some(".mjs")
    )
}

pub fn mime_type_for_url(url: &str) -> &'static std::ffi::CStr {
    let path = path_after_host(url).unwrap_or("");
    match ext_before_query(path) {
        ".js" | ".mjs" => c"application/javascript",
        ".css" => c"text/css",
        ".json" => c"application/json",
        ".html" | ".htm" => c"text/html",
        ".svg" => c"image/svg+xml",
        ".wasm" => c"application/wasm",
        ".png" => c"image/png",
        ".jpg" | ".jpeg" => c"image/jpeg",
        ".gif" => c"image/gif",
        ".ico" => c"image/x-icon",
        ".ttf" => c"font/ttf",
        ".woff" => c"font/woff",
        ".woff2" => c"font/woff2",
        _ => c"application/octet-stream",
    }
}

#[cfg(windows)]
fn steam_install_path() -> PathBuf {
    use std::sync::OnceLock;
    static PATH: OnceLock<PathBuf> = OnceLock::new();
    PATH.get_or_init(|| {
        (|| -> Option<PathBuf> {
            use winreg::enums::HKEY_CURRENT_USER;
            use winreg::RegKey;
            let hkcu = RegKey::predef(HKEY_CURRENT_USER);
            let key = hkcu.open_subkey("Software\\Valve\\Steam").ok()?;
            let path: String = key.get_value("SteamPath").ok()?;
            Some(PathBuf::from(path))
        })()
        .unwrap_or_else(|| PathBuf::from("C:\\Program Files (x86)\\Steam"))
    })
    .clone()
}

#[cfg(not(any(windows, target_os = "macos")))]
fn steam_install_path() -> PathBuf {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/home".into());
    PathBuf::from(home).join(".steam/steam")
}

#[cfg(target_os = "macos")]
fn steam_install_path() -> PathBuf {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/Users".into());
    PathBuf::from(home).join("Library/Application Support/Steam")
}

pub fn local_path_from_lb_url(url: &str) -> Option<PathBuf> {
    let host_end = url.find("steamloopback.host")?;
    let slash = url[host_end..].find('/')? + host_end;
    let path = &url[slash..];
    let path = &path[..path.find('?').unwrap_or(path.len())];
    let rel = path.trim_start_matches('/');

    Some(steam_install_path().join("steamui").join(rel))
}
