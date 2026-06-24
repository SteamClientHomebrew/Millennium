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
use clap::{Parser, Subcommand};
use std::path::{Path, PathBuf};

mod bundler;
mod config;
pub(crate) mod ffi_lint;
pub(crate) mod ffi_types;
mod format;
pub(crate) mod log;
pub(crate) mod lsp;
pub(crate) mod lua_ffi;
pub(crate) mod mep;
mod minify;
mod pack;
pub(crate) mod signing;
pub(crate) mod ts_ffi;
mod verify;
mod watch;

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum BuildMode {
    Debug,
    Release,
}

#[derive(Parser)]
#[command(
    name = "starlight",
    about = "Pack Millennium plugins into .star format",
    version
)]
struct Cli {
    #[arg(short = 'd', long, value_name = "DIR", global = true)]
    dir: Option<PathBuf>,

    #[arg(
        short = 'c',
        long,
        default_value = "millennium.toml",
        value_name = "FILE",
        global = true
    )]
    config: PathBuf,

    #[arg(short = 'o', long = "of", value_name = "FILE", global = true)]
    of: Option<PathBuf>,

    #[arg(long = "if", short = 'i', value_name = "FILE")]
    input_file: Option<PathBuf>,

    #[arg(short = 'q', long, value_name = "QUERY", requires = "input_file")]
    query: Option<String>,

    #[arg(long, group = "build_mode", global = true)]
    debug: bool,

    #[arg(long, group = "build_mode", global = true)]
    release: bool,

    #[command(subcommand)]
    command: Option<Command>,
}

#[derive(Subcommand)]
enum Command {
    Pack,
    Verify { file: PathBuf },
    Inspect { file: PathBuf },
    Watch,
    Lsp,
}

fn resolve_config(dir: Option<&Path>, config: &Path) -> PathBuf {
    match dir {
        Some(d) => d.join("millennium.toml"),
        None => config.to_path_buf(),
    }
}

fn main() {
    let cli = Cli::parse();

    if let (Some(input), Some(q)) = (cli.input_file.as_deref(), cli.query.as_deref()) {
        if let Err(e) = verify::query(input, q) {
            print_error(&e);
            std::process::exit(1);
        }
        return;
    }

    let mode = if cli.debug {
        BuildMode::Debug
    } else {
        BuildMode::Release
    };
    let cfg_path = resolve_config(cli.dir.as_deref(), &cli.config);

    let result = match cli.command.unwrap_or(Command::Pack) {
        Command::Pack => pack::pack(&cfg_path, cli.of.as_deref(), mode).map(|dev| {
            if let Some(d) = dev {
                mep::restart_plugin(&d.plugin_name, &d.socket, d.reload_steamui_when.is_active());
            }
        }),
        Command::Verify { file } => verify::verify(&file),
        Command::Inspect { file } => verify::inspect(&file),
        Command::Watch => watch::watch(&cfg_path, cli.of.as_deref(), mode),
        Command::Lsp => {
            let plugin_dir = cfg_path.parent().unwrap_or(Path::new(".")).to_path_buf();
            crate::config::load(&cfg_path)
                .and_then(|cfg| lsp::install_types(&plugin_dir, env!("CARGO_PKG_VERSION"), &cfg))
        }
    };

    if let Err(e) = result {
        if e.downcast_ref::<bundler::js::BundleCompileError>()
            .is_none()
        {
            print_error(&e);
        }
        std::process::exit(1);
    }
}

fn print_error(e: &anyhow::Error) {
    let mut causes = e.chain();
    let header = causes.next().unwrap().to_string();
    let body: String = causes.map(|c| format!("  caused by: {}\n", c)).collect();
    log::build_error(&header, &body);
}
