const invoke = window.__TAURI__?.core?.invoke;
const $ = id => document.getElementById(id);
let editing = false;

function normalizeInput(raw){
  let value=String(raw||'').trim();
  if(!value)return '';
  if(/^qrx:\/\//i.test(value)||/\.qrx(?:\/|$)/i.test(value))return value;
  if(/^https:\/\//i.test(value))return value;
  if(/^http:\/\//i.test(value))return value;
  if(/\s/.test(value))return 'https://www.google.com/search?q='+encodeURIComponent(value);
  return 'https://'+value;
}
function setMeta(meta){
  if(!meta)return;
  const url=meta.display_url||meta.url||'';
  if(!editing && url && !url.startsWith('tauri://') && !url.includes('tauri.localhost')) $('address').value=url;
  $('tabTitle').textContent=meta.title||url||'New tab';
  $('route').textContent=(meta.route||'HOME').toUpperCase();
  $('status').textContent=meta.status||'QRX Browser';
}
async function navigate(raw){
  const input=normalizeInput(raw);if(!input)return;
  $('status').textContent='Opening…';
  try{setMeta(await invoke('browser_navigate',{input}));}
  catch(e){$('status').textContent='Navigation failed: '+String(e);}
}
$('go').addEventListener('click',()=>navigate($('address').value));
$('address').addEventListener('focus',()=>editing=true);
$('address').addEventListener('blur',()=>editing=false);
$('address').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();editing=false;navigate($('address').value);}});
$('back').addEventListener('click',async()=>{try{await invoke('browser_back');}catch(e){$('status').textContent=String(e)}});
$('forward').addEventListener('click',async()=>{try{await invoke('browser_forward');}catch(e){$('status').textContent=String(e)}});
$('reload').addEventListener('click',async()=>{try{await invoke('browser_reload');}catch(e){$('status').textContent=String(e)}});
$('home').addEventListener('click',async()=>{try{setMeta(await invoke('browser_home'));}catch(e){$('status').textContent=String(e)}});

async function poll(){
  try{setMeta(await invoke('browser_state'));}catch(_){}
}
setInterval(poll,700);
poll();
