const invoke = window.__TAURI__?.core?.invoke;
const $ = id => document.getElementById(id);
let editing = false;
let addressDirty = false;
let routeMode = 'www';
let drawerOpen = false;

function normalizeInput(raw){
  let value=String(raw||'').trim();
  if(!value)return '';
  if(/^qrx:\/\//i.test(value)||/\.qrx(?:\/|$)/i.test(value))return value;
  if(/^https:\/\//i.test(value)||/^http:\/\//i.test(value))return value;
  if(routeMode==='qrx')return 'qrx://'+value.replace(/^\/+/, '');
  if(/\s/.test(value))return 'https://www.google.com/search?q='+encodeURIComponent(value);
  return 'https://'+value;
}
function setMode(mode){
  routeMode=String(mode||'www').toLowerCase()==='qrx'?'qrx':'www';
  $('route').textContent=routeMode.toUpperCase();$('route').dataset.mode=routeMode;
  $('modeWWW').classList.toggle('active',routeMode==='www');$('modeQRX').classList.toggle('active',routeMode==='qrx');
  $('address').placeholder=routeMode==='qrx'?'Enter name.qrx or qrx://name.qrx/path':'Search or enter an HTTPS address';
}
async function chooseMode(mode){try{setMode(await invoke('browser_set_route_mode',{mode}));}catch(e){$('status').textContent=String(e)}}
function setMeta(meta){
  if(!meta)return;const url=meta.display_url||meta.url||'';
  if(!editing && !addressDirty && url && !url.startsWith('tauri://') && !url.includes('tauri.localhost') && meta.route!=='privacy') $('address').value=url;
  if(!editing && !addressDirty && meta.route==='home') $('address').value='';
  $('tabTitle').textContent=meta.title||url||'New tab';
  if(meta.route==='www'||meta.route==='qrx')setMode(meta.route);
  $('status').textContent=meta.status||'QRX Browser';
}
async function navigate(raw){const input=normalizeInput(raw);if(!input)return;$('status').textContent='Opening…';try{const meta=await invoke('browser_navigate',{input});addressDirty=false;editing=false;setMeta(meta);}catch(e){$('status').textContent='Navigation failed: '+String(e);}}
async function newTab(){try{addressDirty=false;editing=false;setMeta(await invoke('browser_new_tab'));}catch(e){$('status').textContent=String(e)}}
async function privacy(){try{addressDirty=false;editing=false;setMeta(await invoke('browser_privacy'));closeDrawer();}catch(e){$('status').textContent=String(e)}}
async function toggleDrawer(){drawerOpen=!drawerOpen;$('drawer').hidden=!drawerOpen;$('menu').setAttribute('aria-expanded',String(drawerOpen));try{await invoke('browser_set_chrome_expanded',{expanded:drawerOpen});}catch(e){$('status').textContent=String(e)}}
async function closeDrawer(){if(!drawerOpen)return;drawerOpen=false;$('drawer').hidden=true;$('menu').setAttribute('aria-expanded','false');try{await invoke('browser_set_chrome_expanded',{expanded:false});}catch(_){}}

$('go').addEventListener('click',()=>navigate($('address').value));
$('address').addEventListener('focus',()=>editing=true);
$('address').addEventListener('input',()=>{editing=true;addressDirty=true;});
$('address').addEventListener('blur',()=>{editing=false;});
$('address').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();editing=false;navigate($('address').value);}});
$('back').addEventListener('click',async()=>{try{await invoke('browser_back');}catch(e){$('status').textContent=String(e)}});
$('forward').addEventListener('click',async()=>{try{await invoke('browser_forward');}catch(e){$('status').textContent=String(e)}});
$('reload').addEventListener('click',async()=>{try{await invoke('browser_reload');}catch(e){$('status').textContent=String(e)}});
$('home').addEventListener('click',newTab);$('newTab').addEventListener('click',newTab);$('privacyTab').addEventListener('click',privacy);$('openPrivacy').addEventListener('click',privacy);
$('menu').addEventListener('click',toggleDrawer);$('route').addEventListener('click',()=>chooseMode(routeMode==='www'?'qrx':'www'));
$('modeWWW').addEventListener('click',()=>chooseMode('www'));$('modeQRX').addEventListener('click',()=>chooseMode('qrx'));
$('clearData').addEventListener('click',async()=>{if(!confirm('Clear cookies, caches and other browsing data for the QRX Browser content WebView?'))return;try{setMeta(await invoke('browser_clear_data'));closeDrawer();$('status').textContent='Browsing data cleared';}catch(e){$('status').textContent=String(e)}});

async function poll(){try{setMeta(await invoke('browser_state'));}catch(_){}}
(async()=>{try{setMode(await invoke('browser_route_mode'));}catch(_){setMode('www')}await poll();})();
setInterval(poll,700);

let lastChromeHeight=0;
async function syncChromeBounds(){
  const chrome=document.querySelector('.chrome'); if(!chrome||!invoke)return;
  const rect=chrome.getBoundingClientRect();
  const height=Math.ceil(rect.bottom + 2); // 2px native WebView safety gap
  if(height===lastChromeHeight)return; lastChromeHeight=height;
  try{await invoke('browser_set_chrome_height',{height});}catch(_){}
}
new ResizeObserver(()=>syncChromeBounds()).observe(document.querySelector('.chrome'));
window.addEventListener('resize',syncChromeBounds);
requestAnimationFrame(syncChromeBounds);
