use std::fs::{self, File};
use std::io::Write;
use std::path::{Path};
use std::process::Command;

fn zip_dir(src: &Path, dest: &Path) -> Result<(), Box<dyn std::error::Error>> {
    let file = File::create(dest)?;
    let mut zip = zip::ZipWriter::new(file);
    let options = zip::write::SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated);

    for entry in walkdir::WalkDir::new(src).into_iter().filter_map(|e| e.ok()) {
        let path = entry.path();
        let name = path.strip_prefix(src)?.to_string_lossy().replace('\\', "/");
        if path.is_dir() {
            if !name.is_empty() {
                zip.add_directory(&name, options)?;
            }
        } else {
            zip.start_file(&name, options)?;
            zip.write_all(&fs::read(path)?)?;
        }
    }
    zip.finish()?;
    Ok(())
}

fn zip_files_with_names(entries: &[(&Path, &str)], dest: &Path) -> Result<(), Box<dyn std::error::Error>> {
    let file = File::create(dest)?;
    let mut zip = zip::ZipWriter::new(file);
    let options = zip::write::SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated);
    for (path, name) in entries {
        zip.start_file(*name, options)?;
        zip.write_all(&fs::read(path)?)?;
    }
    zip.finish()?;
    Ok(())
}

fn main() {
    let repo_root = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let sdk_dir = repo_root.join("src/typescript/sdk");
    let sdk_src = sdk_dir.join("src");
    let lua_types_src = repo_root.join("src/lua_host/api/types");
    let bin_dir = repo_root.join("npm/bin/millennium/types");

    println!("cargo:rerun-if-changed={}", sdk_src.display());
    println!("cargo:rerun-if-changed={}", lua_types_src.display());

    fs::create_dir_all(&bin_dir).expect("failed to create npm/bin");

    let api_out = sdk_dir.join("build/api");
    let _ = fs::remove_dir_all(&api_out);

    let tsc_status = Command::new("bun")
        .args(["x", "tsc", "-p", "tsconfig.api.json"])
        .current_dir(&sdk_dir)
        .status();

    match tsc_status {
        Ok(s) if s.success() => {
            let api_dts = api_out.join("api.d.ts");
            let index_dts = api_out.join("index.d.ts");
            if api_dts.exists() {
                fs::copy(&api_dts, &index_dts).expect("failed to create index.d.ts");
            }
            if api_out.exists() {
                zip_dir(&api_out, &bin_dir.join("types.zip")).expect("failed to zip types");
            }

            let millennium_api_dts = api_out.join("millennium-api.d.ts");

            if millennium_api_dts.exists() {
                let dts = fs::read_to_string(&millennium_api_dts).unwrap_or_default();
                let re = regex::Regex::new(r"(?m)^export\s+(?:declare\s+)?(?:function|const|class|type|enum|interface)\s+(\w+)").unwrap();
                let mut names: Vec<&str> = re
                    .captures_iter(&dts)
                    .filter_map(|c| c.get(1).map(|m| m.as_str()))
                    .collect();
                names.sort_unstable();
                names.dedup();
                let json = format!("[{}]", names.iter().map(|n| format!("\"{}\"", n)).collect::<Vec<_>>().join(","));
                fs::write(bin_dir.join("webkit-exports.json"), json).expect("failed to write webkit-exports.json");
            }
            if millennium_api_dts.exists() {
                let webkit_index = b"export * from './millennium-api';\n";
                let tmp_webkit_index = api_out.join("webkit-index.d.ts");
                fs::write(&tmp_webkit_index, webkit_index).expect("failed to write webkit-index.d.ts");

                zip_files_with_names(
                    &[
                        (&millennium_api_dts, "millennium-api.d.ts"),
                        (&tmp_webkit_index, "index.d.ts"),
                    ],
                    &bin_dir.join("webkit-types.zip"),
                )
                .expect("failed to zip webkit-types");

                let _ = fs::remove_file(&tmp_webkit_index);
            }
        }
        Ok(_) => eprintln!("build.rs: tsc exited with error — type zips not updated"),
        Err(e) => eprintln!("build.rs: could not run bun: {} — type zips not updated", e),
    }

    if lua_types_src.exists() {
        zip_dir(&lua_types_src, &bin_dir.join("lua-types.zip")).expect("failed to zip lua-types");
    }
}
