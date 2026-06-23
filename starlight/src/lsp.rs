use std::fs;
use std::io::{self, Cursor};
use std::path::{Path, PathBuf};

fn types_dir() -> PathBuf {
    std::env::var("STARLIGHT_TYPES_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from("."))
}

fn extract_zip(data: &[u8], dest: &Path) -> anyhow::Result<()> {
    fs::create_dir_all(dest)?;
    let mut archive = zip::ZipArchive::new(Cursor::new(data))?;
    for i in 0..archive.len() {
        let mut file = archive.by_index(i)?;
        let out = dest.join(file.mangled_name());
        if file.is_dir() {
            fs::create_dir_all(&out)?;
        } else {
            if let Some(parent) = out.parent() {
                fs::create_dir_all(parent)?;
            }
            io::copy(&mut file, &mut fs::File::create(&out)?)?;
        }
    }
    Ok(())
}

fn write_tsconfig_paths(tsconfig_path: &Path, ts_dir: &str) -> anyhow::Result<()> {
    let mut json: serde_json::Value = if tsconfig_path.exists() {
        serde_json::from_str(&fs::read_to_string(tsconfig_path)?)?
    } else {
        serde_json::json!({ "compilerOptions": {} })
    };

    let compiler_options = json
        .get_mut("compilerOptions")
        .and_then(|v| v.as_object_mut())
        .ok_or_else(|| anyhow::anyhow!("{}: compilerOptions is not an object", tsconfig_path.display()))?;

    compiler_options.insert("baseUrl".to_string(), serde_json::json!("."));
    compiler_options.insert(
        "paths".to_string(),
        serde_json::json!({ "@steambrew/millennium": [ts_dir] }),
    );

    compiler_options.insert(
        "typeRoots".to_string(),
        serde_json::json!([".millennium/types", "node_modules/@types"]),
    );

    fs::write(tsconfig_path, serde_json::to_string_pretty(&json)?)?;
    Ok(())
}

fn patch_tsconfig(plugin_dir: &Path, cfg: &crate::config::PlgConfig) -> anyhow::Result<()> {
    let has_frontend = cfg.frontend.is_some();
    let has_webkit = cfg.webkit.is_some();

    let root_ts_dir = if has_frontend {
        ".millennium/lsp/ts"
    } else {
        ".millennium/lsp/webkit-ts"
    };
    write_tsconfig_paths(&plugin_dir.join("tsconfig.json"), root_ts_dir)?;

    if has_frontend && has_webkit {
        if let Some(webkit_entry) = cfg.webkit.as_ref().map(|w| &w.entry) {
            let webkit_dir = plugin_dir.join(webkit_entry).parent().map(|p| p.to_path_buf());
            if let Some(webkit_dir) = webkit_dir {
                if webkit_dir != plugin_dir {
                    fs::create_dir_all(&webkit_dir)?;
                    let webkit_tsconfig = webkit_dir.join("tsconfig.json");

                    let depth = webkit_dir.strip_prefix(plugin_dir).map(|p| p.components().count()).unwrap_or(1);
                    let relative_root = "../".repeat(depth) + "tsconfig.json";
                    let mut json = serde_json::json!({
                        "extends": relative_root,
                        "compilerOptions": {}
                    });
                    let compiler_options = json
                        .get_mut("compilerOptions")
                        .and_then(|v| v.as_object_mut())
                        .unwrap();
                    compiler_options.insert("baseUrl".to_string(), serde_json::json!(".."));
                    compiler_options.insert(
                        "paths".to_string(),
                        serde_json::json!({ "@steambrew/millennium": [".millennium/lsp/webkit-ts"] }),
                    );
                    fs::write(&webkit_tsconfig, serde_json::to_string_pretty(&json)?)?;
                }
            }
        }
    }

    Ok(())
}

fn patch_luarc(plugin_dir: &Path) -> anyhow::Result<()> {
    let luarc_path = plugin_dir.join(".luarc.json");
    let mut json: serde_json::Value = if luarc_path.exists() {
        serde_json::from_str(&fs::read_to_string(&luarc_path)?)?
    } else {
        serde_json::json!({})
    };

    let obj = json
        .as_object_mut()
        .ok_or_else(|| anyhow::anyhow!(".luarc.json is not an object"))?;

    let workspace = obj
        .entry("workspace")
        .or_insert_with(|| serde_json::json!({}));

    let has_library = if let Some(arr) = workspace.get_mut("library").and_then(|v| v.as_array_mut()) {
        if !arr.iter().any(|v| v == ".millennium/lsp/lua-types") {
            arr.push(serde_json::json!(".millennium/lsp/lua-types"));
        }
        true
    } else {
        false
    };

    if !has_library {
        workspace["library"] = serde_json::json!([".millennium/lsp/lua-types"]);
    }

    fs::write(&luarc_path, serde_json::to_string_pretty(&json)?)?;
    Ok(())
}

fn patch_gitignore(plugin_dir: &Path) -> anyhow::Result<()> {
    let gitignore_path = plugin_dir.join(".gitignore");
    let entry = ".millennium/lsp/\n";

    if gitignore_path.exists() {
        let contents = fs::read_to_string(&gitignore_path)?;
        if !contents.contains(".millennium/lsp/") {
            fs::write(&gitignore_path, format!("{}{}", contents, entry))?;
        }
    } else {
        fs::write(&gitignore_path, entry)?;
    }
    Ok(())
}

fn patch_package_json(plugin_dir: &Path) -> anyhow::Result<()> {
    let pkg_path = plugin_dir.join("package.json");
    if !pkg_path.exists() {
        return Ok(());
    }

    let mut json: serde_json::Value = serde_json::from_str(&fs::read_to_string(&pkg_path)?)?;
    let obj = json.as_object_mut().ok_or_else(|| anyhow::anyhow!("package.json is not an object"))?;
    let scripts = obj.entry("scripts").or_insert_with(|| serde_json::json!({}));

    if scripts.get("prepare").is_none() {
        scripts["prepare"] = serde_json::json!("starlight lsp");
    }

    fs::write(&pkg_path, serde_json::to_string_pretty(&json)?)?;
    Ok(())
}

/// Install LSP types into `.millennium/lsp/` inside the plugin directory.
pub fn install_types(plugin_dir: &Path, starlight_version: &str, cfg: &crate::config::PlgConfig) -> anyhow::Result<()> {
    let lsp_dir = plugin_dir.join(".millennium/lsp");
    let version_file = lsp_dir.join("version");

    if version_file.exists() {
        if fs::read_to_string(&version_file).map(|v| v.trim().to_string()).unwrap_or_default() == starlight_version {
            return Ok(());
        }
    }

    let types_dir = types_dir();

    // full client types (millennium-api + sharedjscontext)
    let client_zip = types_dir.join("types.zip");
    if client_zip.exists() {
        extract_zip(&fs::read(&client_zip)?, &lsp_dir.join("ts"))?;
    } else {
        crate::log::warn(&format!("lsp: types.zip not found at {}, skipping", client_zip.display()));
    }

    // webkit-restricted types (millennium-api only)
    let webkit_zip = types_dir.join("webkit-types.zip");
    if webkit_zip.exists() {
        extract_zip(&fs::read(&webkit_zip)?, &lsp_dir.join("webkit-ts"))?;
    } else {
        crate::log::warn(&format!("lsp: webkit-types.zip not found at {}, skipping", webkit_zip.display()));
    }

    // lua types
    let lua_zip = types_dir.join("lua-types.zip");
    if lua_zip.exists() {
        extract_zip(&fs::read(&lua_zip)?, &lsp_dir.join("lua-types"))?;
    }

    fs::write(&version_file, starlight_version)?;

    patch_tsconfig(plugin_dir, cfg)?;
    patch_luarc(plugin_dir)?;
    patch_gitignore(plugin_dir)?;
    patch_package_json(plugin_dir)?;

    Ok(())
}
