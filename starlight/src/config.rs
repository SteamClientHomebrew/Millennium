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
use serde::Deserialize;
use std::path::Path;

#[derive(Deserialize)]
pub struct PlgConfig {
    pub plugin: PluginConfig,
    pub backend: Option<BackendConfig>,
    pub frontend: Option<FrontendConfig>,
    pub webkit: Option<WebkitConfig>,
    pub dev: Option<DevConfig>,
    #[serde(default)]
    pub inspect: InspectConfig,
    #[serde(default)]
    pub logger: LoggerConfig,
}

fn default_backend_col() -> [u8; 3] { [160, 90, 180] }
fn default_frontend_col() -> [u8; 3] { [90, 148, 200] }
fn default_webview_col() -> [u8; 3] { [65, 165, 160] }
fn default_padding() -> i32 { -15 }

#[derive(Deserialize)]
pub struct LoggerConfig {
    #[serde(default = "default_backend_col")]
    pub backend_col: [u8; 3],
    #[serde(default = "default_frontend_col")]
    pub frontend_col: [u8; 3],
    #[serde(default = "default_webview_col")]
    pub webview_col: [u8; 3],
    /// Negative = right-align (pad left), positive = left-align (pad right), 0 = no padding.
    #[serde(default = "default_padding")]
    pub padding: i32,
    pub backend_prefix: Option<String>,
    pub frontend_prefix: Option<String>,
    pub webview_prefix: Option<String>,
}

impl Default for LoggerConfig {
    fn default() -> Self {
        Self {
            backend_col: default_backend_col(),
            frontend_col: default_frontend_col(),
            webview_col: default_webview_col(),
            padding: default_padding(),
            backend_prefix: None,
            frontend_prefix: None,
            webview_prefix: None,
        }
    }
}

fn default_depth() -> u32 {
    2
}
fn default_true() -> bool {
    true
}

#[derive(Deserialize)]
pub struct InspectConfig {
    #[serde(default = "default_depth")]
    pub depth: u32,
    #[serde(default = "default_true")]
    pub colors: bool,
    #[serde(default)]
    pub show_hidden: bool,
}

impl Default for InspectConfig {
    fn default() -> Self {
        Self {
            depth: default_depth(),
            colors: true,
            show_hidden: false,
        }
    }
}

#[derive(Deserialize)]
pub struct DevConfig {
    #[serde(default = "default_true")]
    pub auto_restart: bool,
    #[serde(default)]
    pub reload_steamui_when: ReloadFilter,

    pub plugin_name: Option<String>,
    pub socket: Option<String>,
}

#[derive(Deserialize, Clone, Default, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum ReloadFilter {
    #[default]
    Never,
    Backend,
    Frontend,
    Both,
}

impl ReloadFilter {
    pub fn matches(&self, path: &std::path::Path) -> bool {
        let ext = path.extension().and_then(|e| e.to_str()).unwrap_or("");
        match self {
            ReloadFilter::Never => false,
            ReloadFilter::Backend => matches!(ext, "lua"),
            ReloadFilter::Frontend => matches!(ext, "ts" | "tsx" | "js" | "jsx" | "css"),
            ReloadFilter::Both => true,
        }
    }

    pub fn is_active(&self) -> bool {
        *self != ReloadFilter::Never
    }
}

#[derive(Clone)]
pub struct DevRuntime {
    pub plugin_name: String,
    pub socket: String,
    pub auto_restart: bool,
    pub reload_steamui_when: ReloadFilter,
}

#[derive(Deserialize)]
pub struct PluginConfig {
    pub id: String,
    pub name: String,
    pub version: String,
    pub author: String,
    pub description: Option<String>,
}

#[derive(Deserialize)]
pub struct BackendConfig {
    pub entry: String,
    pub sources: Vec<String>,
}

#[derive(Deserialize)]
pub struct FrontendConfig {
    pub entry: String,
    #[serde(default)]
    pub globals: std::collections::HashMap<String, String>,
}

#[derive(Deserialize)]
pub struct WebkitConfig {
    pub entry: String,
    #[serde(default)]
    pub globals: std::collections::HashMap<String, String>,
}

pub fn load(path: &Path) -> anyhow::Result<PlgConfig> {
    let content = std::fs::read_to_string(path)
        .map_err(|e| anyhow::anyhow!("cannot read {}: {}", path.display(), e))?;
    toml::from_str(&content).map_err(|e| anyhow::anyhow!("cannot parse {}: {}", path.display(), e))
}
