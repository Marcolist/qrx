use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::{collections::BTreeMap, fs::{self, File}, io::Read, path::{Component, Path, PathBuf}};
use tauri::Manager;
use zip::ZipArchive;

use crate::{app_data_dir, run_cli};

const QRX_APP_FORMAT: u32 = 1;
const MAX_PACKAGE_BYTES: u64 = 64 * 1024 * 1024;
const MAX_FILE_BYTES: u64 = 16 * 1024 * 1024;
const MAX_FILES: usize = 256;
const MAX_BUNDLE_TEXT: u64 = 8 * 1024 * 1024;

const ALLOWED_PERMISSIONS: &[&str] = &[
    "wallet.identity.read",
    "wallet.balance.read",
    "wallet.payment.request",
    "chain.read",
    "network.status.read",
    "app.storage",
];

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct QrxAppManifest {
    pub format: u32,
    pub id: String,
    pub name: String,
    pub version: String,
    pub author: String,
    pub entry: String,
    #[serde(default)] pub script: Option<String>,
    #[serde(default)] pub style: Option<String>,
    #[serde(default)] pub icon: Option<String>,
    #[serde(default)] pub description: Option<String>,
    #[serde(default = "default_sdk")] pub sdk: String,
    #[serde(default)] pub min_wallet: Option<String>,
    #[serde(default)] pub networks: Vec<String>,
    #[serde(default)] pub permissions: Vec<String>,
}
fn default_sdk() -> String { "1".into() }

#[derive(Debug, Clone, Serialize, Deserialize)]
struct RegistryEntry {
    manifest: QrxAppManifest,
    source: String,
    root: String,
    approved_permissions: Vec<String>,
    enabled: bool,
}

#[derive(Debug, Serialize)]
struct AppListItem {
    manifest: QrxAppManifest,
    source: String,
    approved_permissions: Vec<String>,
    enabled: bool,
}

fn apps_root() -> Result<PathBuf, String> {
    let p = app_data_dir().map_err(String::from)?.join("apps");
    fs::create_dir_all(&p).map_err(|e| e.to_string())?;
    Ok(p)
}
fn registry_path() -> Result<PathBuf, String> { Ok(apps_root()?.join("registry.json")) }
fn developer_mode_path() -> Result<PathBuf, String> { Ok(apps_root()?.join("developer-mode")) }
fn read_registry() -> Result<Vec<RegistryEntry>, String> {
    let p=registry_path()?;
    if !p.exists(){return Ok(Vec::new());}
    serde_json::from_slice(&fs::read(p).map_err(|e|e.to_string())?).map_err(|e|e.to_string())
}
fn write_registry(items:&[RegistryEntry])->Result<(),String>{
    let p=registry_path()?; let tmp=p.with_extension("tmp");
    fs::write(&tmp,serde_json::to_vec_pretty(items).map_err(|e|e.to_string())?).map_err(|e|e.to_string())?;
    #[cfg(target_os="windows")]
    if p.exists(){fs::remove_file(&p).map_err(|e|e.to_string())?;}
    fs::rename(tmp,p).map_err(|e|e.to_string())
}
fn clean_id(id:&str)->Result<String,String>{
    let s=id.trim();
    if s.is_empty()||s.len()>96||s.starts_with('.')||s.ends_with('.')||!s.chars().all(|c|c.is_ascii_alphanumeric()||matches!(c,'.'|'_'|'-')){
        return Err("Invalid QRX app id; use 1-96 ASCII letters, digits, '.', '_' or '-'".into());
    }
    Ok(s.to_ascii_lowercase())
}
fn clean_rel(path:&str)->Result<PathBuf,String>{
    let p=Path::new(path);
    if p.as_os_str().is_empty()||p.is_absolute(){return Err("App file path must be relative".into());}
    let mut out=PathBuf::new();
    for c in p.components(){
        match c{Component::Normal(x)=>out.push(x),_=>return Err("App file path traversal is not allowed".into())}
    }
    Ok(out)
}
fn validate_manifest(mut m:QrxAppManifest)->Result<QrxAppManifest,String>{
    if m.format!=QRX_APP_FORMAT{return Err(format!("Unsupported .qrxapp format {}",m.format));}
    m.id=clean_id(&m.id)?;
    if m.name.trim().is_empty()||m.name.len()>120{return Err("App name is required and must be <=120 characters".into());}
    if m.version.trim().is_empty()||m.version.len()>64{return Err("App version is required".into());}
    if m.author.trim().is_empty()||m.author.len()>120{return Err("App author is required".into());}
    clean_rel(&m.entry)?;
    if let Some(x)=m.script.as_deref(){clean_rel(x)?;}
    if let Some(x)=m.style.as_deref(){clean_rel(x)?;}
    if let Some(x)=m.icon.as_deref(){clean_rel(x)?;}
    if m.sdk!="1"{return Err("This wallet supports QRX Mini SDK v1 only".into());}
    m.permissions.sort();m.permissions.dedup();
    for p in &m.permissions{if !ALLOWED_PERMISSIONS.contains(&p.as_str()){return Err(format!("Unknown or unsupported permission: {p}"));}}
    Ok(m)
}
fn read_manifest_file(path:&Path)->Result<QrxAppManifest,String>{
    let bytes=fs::read(path).map_err(|e|e.to_string())?;
    if bytes.len()>128*1024{return Err("qrx-app.json is too large".into());}
    validate_manifest(serde_json::from_slice(&bytes).map_err(|e|format!("Invalid qrx-app.json: {e}"))?)
}
fn open_archive(path:&Path)->Result<ZipArchive<File>,String>{
    let meta=fs::metadata(path).map_err(|e|e.to_string())?;
    if meta.len()>MAX_PACKAGE_BYTES{return Err(".qrxapp package exceeds 64 MiB foundation limit".into());}
    let f=File::open(path).map_err(|e|e.to_string())?;
    ZipArchive::new(f).map_err(|e|format!("Invalid .qrxapp ZIP container: {e}"))
}
fn manifest_from_archive(path:&Path)->Result<QrxAppManifest,String>{
    let mut z=open_archive(path)?;
    if z.len()>MAX_FILES{return Err(".qrxapp contains too many files".into());}
    let mut f=z.by_name("qrx-app.json").map_err(|_|".qrxapp must contain qrx-app.json at package root".to_string())?;
    if f.size()>128*1024{return Err("qrx-app.json is too large".into());}
    let mut b=Vec::new();f.read_to_end(&mut b).map_err(|e|e.to_string())?;
    validate_manifest(serde_json::from_slice(&b).map_err(|e|format!("Invalid qrx-app.json: {e}"))?)
}
fn approved_subset(manifest:&QrxAppManifest, approved:&[String])->Result<Vec<String>,String>{
    let mut a=approved.to_vec();a.sort();a.dedup();
    for p in &a{if !manifest.permissions.contains(p){return Err(format!("Permission was not requested by app: {p}"));}}
    Ok(a)
}
fn entry_for(app_id:&str)->Result<RegistryEntry,String>{
    let id=clean_id(app_id)?;
    read_registry()?.into_iter().find(|e|e.manifest.id==id&&e.enabled).ok_or_else(||"QRX app is not installed or enabled".into())
}
fn read_text(root:&Path, rel:&str)->Result<String,String>{
    let rp=clean_rel(rel)?;let p=root.join(rp);
    let md=fs::metadata(&p).map_err(|_|format!("App bundle file missing: {rel}"))?;
    if !md.is_file()||md.len()>MAX_BUNDLE_TEXT{return Err(format!("App bundle file invalid or too large: {rel}"));}
    fs::read_to_string(p).map_err(|e|format!("App bundle file must be UTF-8 ({rel}): {e}"))
}
fn permission_for_method(method:&str)->Option<&'static str>{
    match method{
        "wallet.getIdentity"=>Some("wallet.identity.read"),
        "wallet.getBalance"=>Some("wallet.balance.read"),
        "wallet.requestPayment"=>Some("wallet.payment.request"),
        "chain.getHeight"=>Some("chain.read"),
        "network.getStatus"=>Some("network.status.read"),
        "storage.get"|"storage.set"=>Some("app.storage"),
        _=>None,
    }
}
fn require_permission(e:&RegistryEntry, method:&str)->Result<(),String>{
    let p=permission_for_method(method).ok_or_else(||"Unknown QRX Mini SDK method".to_string())?;
    if !e.approved_permissions.iter().any(|x|x==p){return Err(format!("Permission denied: {p}"));}
    Ok(())
}

#[tauri::command]
pub fn qrx_app_inspect_package(package_path:String)->Result<Value,String>{
    let p=PathBuf::from(package_path);let m=manifest_from_archive(&p)?;
    Ok(serde_json::json!({"manifest":m,"allowed_permissions":ALLOWED_PERMISSIONS,"signature_status":"not-verified-in-0.0.9","source":"sideload"}))
}
#[tauri::command]
pub fn qrx_app_install(package_path:String,approved_permissions:Vec<String>)->Result<Value,String>{
    let src=PathBuf::from(&package_path);let m=manifest_from_archive(&src)?;let approved=approved_subset(&m,&approved_permissions)?;
    let base=apps_root()?.join("installed").join(&m.id);let final_dir=base.join(&m.version);let tmp=base.join(format!(".install-{}",std::process::id()));
    if tmp.exists(){fs::remove_dir_all(&tmp).map_err(|e|e.to_string())?;}fs::create_dir_all(&tmp).map_err(|e|e.to_string())?;
    let mut z=open_archive(&src)?;let mut total=0u64;
    for i in 0..z.len(){
        let mut f=z.by_index(i).map_err(|e|e.to_string())?;
        if f.size()>MAX_FILE_BYTES{return Err(format!("App file too large: {}",f.name()));}
        total=total.saturating_add(f.size());if total>MAX_PACKAGE_BYTES{return Err("Expanded .qrxapp exceeds 64 MiB foundation limit".into());}
        if let Some(mode)=f.unix_mode(){if mode&0o170000==0o120000{return Err("Symlinks are not allowed inside .qrxapp packages".into());}}
        let rel=f.enclosed_name().ok_or_else(||"Unsafe path in .qrxapp package".to_string())?.to_path_buf();
        let out=tmp.join(rel);
        if f.is_dir(){fs::create_dir_all(&out).map_err(|e|e.to_string())?;continue;}
        if let Some(parent)=out.parent(){fs::create_dir_all(parent).map_err(|e|e.to_string())?;}
        let mut of=File::create(&out).map_err(|e|e.to_string())?;std::io::copy(&mut f,&mut of).map_err(|e|e.to_string())?;
    }
    let installed_manifest=read_manifest_file(&tmp.join("qrx-app.json"))?;
    if installed_manifest.id!=m.id||installed_manifest.version!=m.version{return Err("Package manifest changed during extraction".into());}
    if !tmp.join(clean_rel(&m.entry)?).is_file(){return Err("App entry file is missing".into());}
    fs::create_dir_all(&base).map_err(|e|e.to_string())?;
    if final_dir.exists(){fs::remove_dir_all(&final_dir).map_err(|e|e.to_string())?;}
    fs::rename(&tmp,&final_dir).map_err(|e|e.to_string())?;
    let mut r=read_registry()?;r.retain(|e|e.manifest.id!=m.id);
    r.push(RegistryEntry{manifest:m.clone(),source:"sideload".into(),root:final_dir.to_string_lossy().to_string(),approved_permissions:approved.clone(),enabled:true});write_registry(&r)?;
    Ok(serde_json::json!({"installed":true,"manifest":m,"approved_permissions":approved,"signature_status":"not-verified-in-0.0.9"}))
}
#[tauri::command]
pub fn qrx_app_list()->Result<Value,String>{
    let items:Vec<AppListItem>=read_registry()?.into_iter().map(|e|AppListItem{manifest:e.manifest,source:e.source,approved_permissions:e.approved_permissions,enabled:e.enabled}).collect();
    Ok(serde_json::json!({"format":QRX_APP_FORMAT,"apps":items,"developer_mode":developer_mode_path()?.exists(),"directory":"deferred-to-0.0.10"}))
}
#[tauri::command]
pub fn qrx_app_uninstall(app_id:String)->Result<Value,String>{
    let id=clean_id(&app_id)?;let mut r=read_registry()?;let mut roots=Vec::new();
    r.retain(|e|{if e.manifest.id==id{if e.source=="sideload"{roots.push(e.root.clone());}false}else{true}});write_registry(&r)?;
    for x in roots{let p=PathBuf::from(x);if p.exists(){let _=fs::remove_dir_all(p);}}
    Ok(serde_json::json!({"uninstalled":true,"id":id}))
}
#[tauri::command]
pub fn qrx_app_set_developer_mode(enabled:bool)->Result<Value,String>{
    let p=developer_mode_path()?;
    if enabled{
        fs::write(&p,b"enabled\n").map_err(|e|e.to_string())?;
    }else{
        if p.exists(){fs::remove_file(&p).map_err(|e|e.to_string())?;}
        let mut r=read_registry()?;r.retain(|e|e.source!="developer");write_registry(&r)?;
    }
    Ok(serde_json::json!({"developer_mode":enabled}))
}
#[tauri::command]
pub fn qrx_app_inspect_dev_folder(folder:String)->Result<Value,String>{
    if !developer_mode_path()?.exists(){return Err("Enable Developer Mode before loading an unpacked app".into());}
    let root=PathBuf::from(folder);let m=read_manifest_file(&root.join("qrx-app.json"))?;
    if !root.join(clean_rel(&m.entry)?).is_file(){return Err("Developer app entry file is missing".into());}
    Ok(serde_json::json!({"manifest":m,"source":"developer","warning":"Developer apps are live local folders; changes are visible on reload."}))
}
#[tauri::command]
pub fn qrx_app_register_dev(folder:String,approved_permissions:Vec<String>)->Result<Value,String>{
    if !developer_mode_path()?.exists(){return Err("Developer Mode is disabled".into());}
    let root=PathBuf::from(&folder).canonicalize().map_err(|e|e.to_string())?;let m=read_manifest_file(&root.join("qrx-app.json"))?;let approved=approved_subset(&m,&approved_permissions)?;
    if !root.join(clean_rel(&m.entry)?).is_file(){return Err("Developer app entry file is missing".into());}
    let mut r=read_registry()?;r.retain(|e|e.manifest.id!=m.id);r.push(RegistryEntry{manifest:m.clone(),source:"developer".into(),root:root.to_string_lossy().to_string(),approved_permissions:approved.clone(),enabled:true});write_registry(&r)?;
    Ok(serde_json::json!({"registered":true,"manifest":m,"approved_permissions":approved}))
}
#[tauri::command]
pub fn qrx_app_load_bundle(app_id:String)->Result<Value,String>{
    let e=entry_for(&app_id)?;let root=PathBuf::from(&e.root);let html=read_text(&root,&e.manifest.entry)?;
    let script=match e.manifest.script.as_deref(){Some(x)=>read_text(&root,x)?,None=>String::new()};
    let style=match e.manifest.style.as_deref(){Some(x)=>read_text(&root,x)?,None=>String::new()};
    Ok(serde_json::json!({"manifest":e.manifest,"source":e.source,"approved_permissions":e.approved_permissions,"html":html,"script":script,"style":style}))
}
#[tauri::command]
pub fn open_qrx_app_window(app:tauri::AppHandle,app_id:String,network:Option<String>,wallet:Option<String>)->Result<String,String>{
    let e=entry_for(&app_id)?;let label=format!("qrx-app-{}",e.manifest.id.replace('.', "-"));
    if let Some(w)=app.get_window(&label){w.show().map_err(|e|e.to_string())?;w.set_focus().map_err(|e|e.to_string())?;return Ok(format!("{} focused.",e.manifest.name));}
    let url=format!("app-host/index.html?app={}&network={}&wallet={}",e.manifest.id,network.unwrap_or_else(||"alpha".into()),wallet.unwrap_or_else(||"node1".into()));
    tauri::WindowBuilder::new(&app,label,tauri::WindowUrl::App(url.into())).title(&e.manifest.name).inner_size(1180.0,780.0).min_inner_size(720.0,520.0).resizable(true).center().build().map_err(|e|e.to_string())?;
    Ok(format!("{} opened in the QRX App Sandbox.",e.manifest.name))
}
fn app_storage_path(id:&str)->Result<PathBuf,String>{let p=apps_root()?.join("storage");fs::create_dir_all(&p).map_err(|e|e.to_string())?;Ok(p.join(format!("{}.json",clean_id(id)?)))}
fn read_storage(id:&str)->Result<BTreeMap<String,Value>,String>{let p=app_storage_path(id)?;if !p.exists(){return Ok(BTreeMap::new());}serde_json::from_slice(&fs::read(p).map_err(|e|e.to_string())?).map_err(|e|e.to_string())}
fn write_storage(id:&str,v:&BTreeMap<String,Value>)->Result<(),String>{let b=serde_json::to_vec_pretty(v).map_err(|e|e.to_string())?;if b.len()>64*1024{return Err("App storage quota exceeded (64 KiB in 0.0.9)".into());}fs::write(app_storage_path(id)?,b).map_err(|e|e.to_string())}

#[tauri::command]
pub fn qrx_app_bridge_call(app:tauri::AppHandle,app_id:String,method:String,params:Value,network:String,wallet:String)->Result<Value,String>{
    let e=entry_for(&app_id)?;require_permission(&e,&method)?;
    match method.as_str(){
        "wallet.getIdentity"=>{let r=run_cli(Some(&app),&network,&wallet,&["getwalletinfo"],None).map_err(String::from)?.result;Ok(serde_json::json!({"network":network,"wallet":wallet,"address":r.get("address").cloned().unwrap_or(Value::Null)}))},
        "wallet.getBalance"=>Ok(run_cli(Some(&app),&network,&wallet,&["getbalance"],None).map_err(String::from)?.result),
        "chain.getHeight"=>Ok(run_cli(Some(&app),&network,&wallet,&["getblockcount"],None).map_err(String::from)?.result),
        "network.getStatus"=>Ok(run_cli(Some(&app),&network,&wallet,&["getnetworkinfo"],None).map_err(String::from)?.result),
        "wallet.requestPayment"=>{
            let recipient=params.get("recipient").and_then(Value::as_str).unwrap_or("").trim();let amount=params.get("amount").and_then(Value::as_str).unwrap_or("").trim();
            if recipient.is_empty()||amount.is_empty(){return Err("Payment request needs recipient and amount strings".into());}
            let payload=serde_json::json!({"app_id":e.manifest.id,"app_name":e.manifest.name,"recipient":recipient,"amount":amount,"memo":params.get("memo").and_then(Value::as_str).unwrap_or(""),"network":network});
            if let Some(w)=app.get_window("main"){w.emit("qrx-app-payment-request",payload.clone()).map_err(|e|e.to_string())?;w.show().map_err(|e|e.to_string())?;let _=w.set_focus();}
            Ok(serde_json::json!({"queued_for_wallet_confirmation":true,"request":payload}))
        },
        "storage.get"=>{let key=params.get("key").and_then(Value::as_str).unwrap_or("");if key.is_empty()||key.len()>128{return Err("Invalid storage key".into());}Ok(read_storage(&e.manifest.id)?.get(key).cloned().unwrap_or(Value::Null))},
        "storage.set"=>{let key=params.get("key").and_then(Value::as_str).unwrap_or("");if key.is_empty()||key.len()>128{return Err("Invalid storage key".into());}let mut s=read_storage(&e.manifest.id)?;s.insert(key.to_string(),params.get("value").cloned().unwrap_or(Value::Null));write_storage(&e.manifest.id,&s)?;Ok(serde_json::json!({"saved":true}))},
        _=>Err("Unknown QRX Mini SDK method".into()),
    }
}
