#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use base64::{engine::general_purpose, Engine as _};
use serde::Serialize;
use serde_json::Value;
use std::{
    fs,
    path::{Path, PathBuf},
    process::Command,
    sync::Mutex,
};
use tauri::{
    webview::{PageLoadEvent, WebviewBuilder},
    window::WindowBuilder,
    AppHandle, LogicalPosition, LogicalSize, Manager, State, WebviewUrl,
};

const CHROME_HEIGHT: f64 = 128.0;
const CHROME_HEIGHT_EXPANDED: f64 = 244.0;

struct BrowserState {
    network: String,
    wallet: String,
    home_url: Mutex<Option<String>>,
    privacy_url: Mutex<Option<String>>,
    route_mode: Mutex<String>,
    chrome_expanded: Mutex<bool>,
    chrome_height: Mutex<f64>,
    meta: Mutex<BrowserMeta>,
}

#[derive(Serialize, Clone)]
struct BrowserMeta {
    route: String,
    url: String,
    display_url: String,
    title: String,
    status: String,
}

fn sanitize_wallet_name(raw: &str) -> Result<String, String> {
    let s = raw.trim();
    if s.is_empty() || s.len() > 64 {
        return Err("Invalid wallet name".into());
    }
    if !s.chars().all(|c| c.is_ascii_alphanumeric() || matches!(c, '-' | '_' | '.')) {
        return Err("Wallet name contains unsupported characters".into());
    }
    Ok(s.to_string())
}

fn parse_context_args() -> (String, String) {
    let mut network = "alpha".to_string();
    let mut wallet = "default".to_string();
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--network" => if let Some(v) = args.next() { network = v; },
            "--wallet" => if let Some(v) = args.next() { wallet = v; },
            _ => {}
        }
    }
    let network = match network.as_str() {
        "mainnet" | "alpha" | "testnet" | "regtest" => network,
        _ => "alpha".into(),
    };
    let wallet = sanitize_wallet_name(&wallet).unwrap_or_else(|_| "default".into());
    (network, wallet)
}

fn target_sidecar_name(base: &str) -> String {
    let arch = if cfg!(target_arch = "x86_64") { "x86_64" } else if cfg!(target_arch = "aarch64") { "aarch64" } else { "unknown" };
    let platform = if cfg!(target_os = "windows") { "pc-windows-msvc" } else if cfg!(target_os = "macos") { "apple-darwin" } else { "unknown-linux-gnu" };
    let ext = if cfg!(target_os = "windows") { ".exe" } else { "" };
    format!("{base}-{arch}-{platform}{ext}")
}

fn find_binary(base: &str) -> Result<PathBuf, String> {
    let suffixed = target_sidecar_name(base);
    let mut candidates = Vec::new();
    if let Ok(dir) = std::env::var("QRX_BIN_DIR") {
        candidates.push(PathBuf::from(&dir).join(&suffixed));
        candidates.push(PathBuf::from(&dir).join(base));
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(parent) = exe.parent() {
            candidates.push(parent.join(&suffixed));
            candidates.push(parent.join(base));
            candidates.push(parent.join("../Resources").join(&suffixed));
            candidates.push(parent.join("../Resources").join(base));
        }
    }
    if let Ok(cwd) = std::env::current_dir() {
        candidates.push(cwd.join("GUIWALLET/src-tauri/bin").join(&suffixed));
        candidates.push(cwd.join("GUIWALLET/src-tauri/bin").join(base));
        candidates.push(cwd.join("bin").join(&suffixed));
        candidates.push(cwd.join("bin").join(base));
    }
    candidates.into_iter().find(|p| p.exists()).ok_or_else(|| format!("Could not find QRX sidecar {base}"))
}

fn qrx_data_dir() -> Result<PathBuf, String> {
    let home = dirs::home_dir().ok_or_else(|| "Could not resolve home directory".to_string())?;
    Ok(home.join(".qrx"))
}

fn run_cli(state: &BrowserState, args: &[&str]) -> Result<Value, String> {
    let cli = find_binary("qrx-cli")?;
    let data_dir = qrx_data_dir()?;
    let output = Command::new(cli)
        .arg("--network").arg(&state.network)
        .arg("--datadir").arg(data_dir)
        .arg("--wallet").arg(&state.wallet)
        .args(args)
        .env("QRX_PASSPHRASE", "")
        .output()
        .map_err(|e| format!("Could not start qrx-cli: {e}"))?;
    if !output.status.success() {
        let err = String::from_utf8_lossy(&output.stderr).trim().to_string();
        let out = String::from_utf8_lossy(&output.stdout).trim().to_string();
        return Err(if !err.is_empty() { err } else { out });
    }
    let stdout = String::from_utf8_lossy(&output.stdout);
    let line = stdout.lines().find(|l| l.trim_start().starts_with('{'))
        .ok_or_else(|| format!("Unexpected qrx-cli output: {stdout}"))?;
    let wrapper: Value = serde_json::from_str(line).map_err(|e| e.to_string())?;
    Ok(wrapper.get("result").cloned().unwrap_or(wrapper))
}

fn mime_for(path: &Path) -> &'static str {
    let p = path.to_string_lossy().to_ascii_lowercase();
    if p.ends_with(".html") || p.ends_with(".htm") { "text/html;charset=utf-8" }
    else if p.ends_with(".css") { "text/css;charset=utf-8" }
    else if p.ends_with(".js") || p.ends_with(".mjs") { "text/javascript;charset=utf-8" }
    else if p.ends_with(".json") { "application/json;charset=utf-8" }
    else if p.ends_with(".svg") { "image/svg+xml" }
    else if p.ends_with(".png") { "image/png" }
    else if p.ends_with(".jpg") || p.ends_with(".jpeg") { "image/jpeg" }
    else if p.ends_with(".webp") { "image/webp" }
    else { "application/octet-stream" }
}

fn store_meta(state: &BrowserState, meta: &BrowserMeta) {
    if let Ok(mut slot) = state.meta.lock() {
        *slot = meta.clone();
    }
}

fn content(app: &AppHandle) -> Result<tauri::Webview, String> {
    app.get_webview("browser-content").ok_or_else(|| "Browser content WebView is unavailable".into())
}

fn meta_for(app: &AppHandle, route: &str, status: &str) -> Result<BrowserMeta, String> {
    let view = content(app)?;
    let url = view.url().map_err(|e| e.to_string())?.to_string();
    let display = if url.starts_with("data:") || url.contains("tauri.localhost") || url.starts_with("tauri://") { String::new() } else { url.clone() };
    Ok(BrowserMeta {
        route: route.to_string(),
        url,
        display_url: display,
        title: if route == "home" { "New tab".into() } else { route.to_uppercase() },
        status: status.to_string(),
    })
}

#[tauri::command]
async fn browser_navigate(app: AppHandle, state: State<'_, BrowserState>, input: String) -> Result<BrowserMeta, String> {
    let raw = input.trim();
    if raw.is_empty() { return Err("Address is empty".into()); }
    let lower = raw.to_ascii_lowercase();
    let explicit_qrx = lower.starts_with("qrx://") || lower.ends_with(".qrx") || lower.contains(".qrx/");
    let explicit_www = lower.starts_with("https://") || lower.starts_with("http://");
    let route_mode = state.route_mode.lock().map_err(|_| "Browser state lock poisoned".to_string())?.clone();
    let is_qrx = explicit_qrx || (!explicit_www && route_mode == "qrx");
    let view = content(&app)?;

    if is_qrx {
        run_cli(&state, &["resolvebrowserinput", raw])?;
        let routed = if explicit_qrx { raw.to_string() } else { format!("qrx://{}", raw.trim_start_matches('/')) };
        let without_scheme = routed.strip_prefix("qrx://").or_else(|| routed.strip_prefix("QRX://")).unwrap_or(routed.as_str());
        let (domain, path) = match without_scheme.split_once('/') {
            Some((d, p)) => (d.to_ascii_lowercase(), if p.is_empty() { "index.html" } else { p }),
            None => (without_scheme.to_ascii_lowercase(), "index.html"),
        };
        let result = run_cli(&state, &["fetchqrxsite", domain.as_str(), path])?;
        let cache_path = result.get("file_cache_path").and_then(Value::as_str)
            .ok_or_else(|| "Verified QRX-Net cache path missing".to_string())?;
        let data = fs::read(cache_path).map_err(|e| format!("Could not read verified QRX page: {e}"))?;
        let mime = mime_for(Path::new(cache_path));
        if !mime.starts_with("text/html") {
            return Err(format!("Browser main document must be HTML, got {mime}"));
        }
        let data_url = format!("data:{mime};base64,{}", general_purpose::STANDARD.encode(data));
        let url = data_url.parse().map_err(|e| format!("Could not build verified QRX page URL: {e}"))?;
        view.navigate(url).map_err(|e| e.to_string())?;
        let meta = BrowserMeta {
            route: "qrx".into(),
            url: data_url,
            display_url: format!("qrx://{domain}/{path}"),
            title: domain,
            status: "QRX VERIFIED · local resolver/cache · no DNS fallback".into(),
        };
        store_meta(&state, &meta);
        return Ok(meta);
    }

    let https = if lower.starts_with("https://") {
        raw.to_string()
    } else if lower.starts_with("http://") {
        return Err("QRX Browser blocks plain HTTP. Use HTTPS.".into());
    } else {
        format!("https://{raw}")
    };
    let url: tauri::Url = https.parse().map_err(|e| format!("Invalid HTTPS URL: {e}"))?;
    if url.scheme() != "https" { return Err("Only HTTPS is allowed on the WWW route".into()); }
    view.navigate(url).map_err(|e| e.to_string())?;
    let meta = BrowserMeta {
        route: "www".into(),
        url: https.clone(),
        display_url: https,
        title: "WWW".into(),
        status: "WWW · native content WebView".into(),
    };
    store_meta(&state, &meta);
    Ok(meta)
}

#[tauri::command]
async fn browser_new_tab(app: AppHandle, state: State<'_, BrowserState>) -> Result<BrowserMeta, String> {
    browser_home(app, state).await
}

#[tauri::command]
async fn browser_privacy(app: AppHandle, state: State<'_, BrowserState>) -> Result<BrowserMeta, String> {
    let privacy = state.privacy_url.lock().map_err(|_| "Browser state lock poisoned".to_string())?.clone()
        .ok_or_else(|| "Browser privacy URL not initialized".to_string())?;
    let url: tauri::Url = privacy.parse().map_err(|e| format!("Invalid Browser privacy URL: {e}"))?;
    content(&app)?.navigate(url).map_err(|e| e.to_string())?;
    let meta = BrowserMeta {
        route: "privacy".into(),
        url: privacy,
        display_url: "qrx://browser/privacy".into(),
        title: "Privacy".into(),
        status: "Privacy Center · QRX/WWW isolation and local browser controls".into(),
    };
    store_meta(&state, &meta);
    Ok(meta)
}

#[tauri::command]
async fn browser_clear_data(app: AppHandle, state: State<'_, BrowserState>) -> Result<BrowserMeta, String> {
    content(&app)?.clear_all_browsing_data().map_err(|e| format!("Could not clear browsing data: {e}"))?;
    browser_home(app, state).await
}

#[tauri::command]
async fn browser_set_route_mode(state: State<'_, BrowserState>, mode: String) -> Result<String, String> {
    let normalized = mode.trim().to_ascii_lowercase();
    if normalized != "www" && normalized != "qrx" {
        return Err("Route mode must be WWW or QRX".into());
    }
    *state.route_mode.lock().map_err(|_| "Browser state lock poisoned".to_string())? = normalized.clone();
    Ok(normalized)
}

#[tauri::command]
async fn browser_route_mode(state: State<'_, BrowserState>) -> Result<String, String> {
    state.route_mode.lock().map_err(|_| "Browser state lock poisoned".to_string()).map(|m| m.clone())
}

#[tauri::command]
async fn browser_set_chrome_expanded(app: AppHandle, state: State<'_, BrowserState>, expanded: bool) -> Result<(), String> {
    *state.chrome_expanded.lock().map_err(|_| "Browser state lock poisoned".to_string())? = expanded;
    let window = app.get_window("qrx-browser").ok_or_else(|| "Browser window unavailable".to_string())?;
    let chrome = app.get_webview("browser-chrome").ok_or_else(|| "Browser chrome unavailable".to_string())?;
    let content = app.get_webview("browser-content").ok_or_else(|| "Browser content unavailable".to_string())?;
    resize_children(&window, &chrome, &content, if expanded { CHROME_HEIGHT_EXPANDED } else { CHROME_HEIGHT });
    Ok(())
}


#[tauri::command]
async fn browser_set_chrome_height(app: AppHandle, state: State<'_, BrowserState>, height: f64) -> Result<(), String> {
    let h = height.clamp(CHROME_HEIGHT, 420.0);
    *state.chrome_height.lock().map_err(|_| "Browser state lock poisoned".to_string())? = h;
    let window = app.get_window("qrx-browser").ok_or_else(|| "Browser window unavailable".to_string())?;
    let chrome = app.get_webview("browser-chrome").ok_or_else(|| "Browser chrome unavailable".to_string())?;
    let content = app.get_webview("browser-content").ok_or_else(|| "Browser content unavailable".to_string())?;
    resize_children(&window, &chrome, &content, h);
    Ok(())
}

#[tauri::command]
async fn browser_back(app: AppHandle) -> Result<(), String> {
    content(&app)?.eval("history.back()").map_err(|e| e.to_string())
}

#[tauri::command]
async fn browser_forward(app: AppHandle) -> Result<(), String> {
    content(&app)?.eval("history.forward()").map_err(|e| e.to_string())
}

#[tauri::command]
async fn browser_reload(app: AppHandle) -> Result<(), String> {
    content(&app)?.reload().map_err(|e| e.to_string())
}

#[tauri::command]
async fn browser_home(app: AppHandle, state: State<'_, BrowserState>) -> Result<BrowserMeta, String> {
    let home = state.home_url.lock().map_err(|_| "Browser state lock poisoned".to_string())?.clone()
        .ok_or_else(|| "Browser home URL not initialized".to_string())?;
    let url: tauri::Url = home.parse().map_err(|e| format!("Invalid Browser home URL: {e}"))?;
    content(&app)?.navigate(url).map_err(|e| e.to_string())?;
    let meta = meta_for(&app, "home", "QRX Browser · two isolated routes")?;
    store_meta(&state, &meta);
    Ok(meta)
}

#[tauri::command]
async fn browser_state(app: AppHandle, state: State<'_, BrowserState>) -> Result<BrowserMeta, String> {
    let view = content(&app)?;
    let url = view.url().map_err(|e| e.to_string())?.to_string();
    let privacy = state.privacy_url.lock().map_err(|_| "Browser state lock poisoned".to_string())?.clone();
    if privacy.as_deref() == Some(url.as_str()) || url.ends_with("/privacy.html") {
        let meta = BrowserMeta {
            route: "privacy".into(),
            url: url.clone(),
            display_url: "qrx://browser/privacy".into(),
            title: "Privacy".into(),
            status: "Privacy Center · QRX/WWW isolation and local browser controls".into(),
        };
        store_meta(&state, &meta);
        return Ok(meta);
    }
    if url.starts_with("https://") {
        let meta = BrowserMeta {
            route: "www".into(),
            url: url.clone(),
            display_url: url.clone(),
            title: url.clone(),
            status: "WWW · native content WebView".into(),
        };
        store_meta(&state, &meta);
        return Ok(meta);
    }
    if url.starts_with("data:") {
        return state.meta.lock()
            .map_err(|_| "Browser state lock poisoned".to_string())
            .map(|m| m.clone());
    }
    let meta = BrowserMeta {
        route: "home".into(),
        url,
        display_url: String::new(),
        title: "New tab".into(),
        status: "QRX Browser · two isolated routes".into(),
    };
    store_meta(&state, &meta);
    Ok(meta)
}

fn resize_children(window: &tauri::Window, chrome: &tauri::Webview, content: &tauri::Webview, chrome_height: f64) {
    let Ok(size) = window.inner_size() else { return; };
    let Ok(scale) = window.scale_factor() else { return; };
    let logical = size.to_logical::<f64>(scale);
    let content_h = (logical.height - chrome_height).max(1.0);
    let _ = chrome.set_position(LogicalPosition::new(0.0, 0.0));
    let _ = chrome.set_size(LogicalSize::new(logical.width, chrome_height));
    let _ = content.set_position(LogicalPosition::new(0.0, chrome_height));
    let _ = content.set_size(LogicalSize::new(logical.width, content_h));
}

fn main() {
    let (network, wallet) = parse_context_args();

    tauri::Builder::default()
        .manage(BrowserState {
            network,
            wallet,
            home_url: Mutex::new(None),
            privacy_url: Mutex::new(None),
            route_mode: Mutex::new("www".into()),
            chrome_expanded: Mutex::new(false),
            chrome_height: Mutex::new(CHROME_HEIGHT),
            meta: Mutex::new(BrowserMeta {
                route: "home".into(),
                url: String::new(),
                display_url: String::new(),
                title: "New tab".into(),
                status: "QRX Browser · two isolated routes".into(),
            }),
        })
        .invoke_handler(tauri::generate_handler![
            browser_navigate,
            browser_new_tab,
            browser_privacy,
            browser_clear_data,
            browser_set_route_mode,
            browser_route_mode,
            browser_set_chrome_expanded,
            browser_set_chrome_height,
            browser_back,
            browser_forward,
            browser_reload,
            browser_home,
            browser_state
        ])
        .setup(|app| {
            let window = WindowBuilder::new(app, "qrx-browser")
                .title("QRX Browser")
                .inner_size(1440.0, 900.0)
                .min_inner_size(960.0, 640.0)
                .resizable(true)
                .center()
                .build()?;

            let chrome_builder = WebviewBuilder::new("browser-chrome", WebviewUrl::App("index.html".into()))
                .on_navigation(|url| {
                    url.scheme() == "tauri" || url.host_str() == Some("tauri.localhost")
                });
            let chrome = window.add_child(
                chrome_builder,
                LogicalPosition::new(0.0, 0.0),
                LogicalSize::new(1440.0, CHROME_HEIGHT),
            )?;

            let content_builder = WebviewBuilder::new("browser-content", WebviewUrl::App("home.html".into()));
            #[cfg(target_os = "macos")]
            let content_builder = content_builder.data_store_identifier(*b"QRXBrowserStore1");
            let content_builder = content_builder
                .on_navigation(|url| {
                    matches!(url.scheme(), "tauri" | "https" | "data")
                        || url.host_str() == Some("tauri.localhost")
                })
                .on_page_load(|webview, payload| {
                    if matches!(payload.event(), PageLoadEvent::Finished) {
                        let window = webview.window();
                        let _ = window.set_title(&format!("QRX Browser · {}", payload.url()));
                    }
                });
            let content = window.add_child(
                content_builder,
                LogicalPosition::new(0.0, CHROME_HEIGHT),
                LogicalSize::new(1440.0, 900.0 - CHROME_HEIGHT),
            )?;

            if let Ok(home) = content.url() {
                let home_string = home.to_string();
                if let Ok(mut slot) = app.state::<BrowserState>().home_url.lock() {
                    *slot = Some(home_string.clone());
                }
                if let Ok(mut slot) = app.state::<BrowserState>().privacy_url.lock() {
                    *slot = Some(home_string.replace("home.html", "privacy.html"));
                }
            }

            resize_children(&window, &chrome, &content, CHROME_HEIGHT);
            let w = window.clone();
            let c1 = chrome.clone();
            let c2 = content.clone();
            let app_handle = app.handle().clone();
            window.on_window_event(move |event| {
                if matches!(event, tauri::WindowEvent::Resized(_) | tauri::WindowEvent::ScaleFactorChanged { .. }) {
                    let state = app_handle.state::<BrowserState>();
                    let h = state.chrome_height.lock().map(|v| *v).unwrap_or(CHROME_HEIGHT);
                    resize_children(&w, &c1, &c2, h);
                }
            });
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running QRX Browser");
}
