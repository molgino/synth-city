// Seite /sync: Bruecke fuer den Sync der Handy-App (https) mit dem Geraet (http).
// Die App oeffnet diese Seite per window.open und schickt Nachrichten (postMessage, kein Netzwerkzugriff -> kein
// "Mixed Content"). Ablauf in wenigen grossen Schritten, damit es auch klappt, wenn iOS ein Fenster kurz anhaelt:
//   App -> hello            Seite fragt "Erlauben?" (zeigt die Herkunft der App)
//   Seite -> manifest       Dateiliste des Geraets (/api/manifest)
//   App -> plan {get, put}  Seite laedt hoch (put) und holt die gewuenschten Dateien (get)
//   Seite -> result         Dateien + neue Liste; App schreibt, ggf. noch ein plan, zum Schluss done
// Direkt geoeffnet (ohne App): alles als ZIP herunterladen oder ein ZIP einspielen.
extern const char WEB_SYNC[];
const char WEB_SYNC[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#080612"><title>SYNTH CITY – Sync</title>
<style>
:root{--bg:#080612;--panel:#141026;--panel2:#20183a;--line:#3a2f60;--text:#d6eeff;--dim:#8a80b0;--acc:#ff2e88;--cy:#00e0ff;--gr:#2bff9a}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;background:linear-gradient(180deg,#080612,#140a2a 60%,#2a0a3a) fixed;color:var(--text);font:15px/1.5 ui-monospace,Menlo,Consolas,monospace;padding:max(16px,env(safe-area-inset-top)) 16px 24px}
h1{font-size:18px;letter-spacing:.18em;margin:0 0 14px;color:#fff;text-shadow:-2px 0 var(--acc),2px 0 var(--cy)}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:16px;margin:0 0 14px;max-width:640px}
button,.btn{display:inline-block;background:var(--panel2);color:var(--text);border:1px solid var(--line);border-radius:10px;font:inherit;padding:10px 16px;margin:6px 8px 0 0;cursor:pointer}
button.go{background:var(--acc);border-color:var(--acc);color:#fff;font-weight:700}
button:disabled{opacity:.5}
b.org{color:var(--cy);word-break:break-all}
#log{white-space:pre-wrap;color:var(--dim);font-size:13px;margin-top:10px;max-height:40vh;overflow:auto}
progress{width:100%;height:10px;margin-top:10px;accent-color:var(--acc)}
.ok{color:var(--gr)}.bad{color:#ff4f6a}
label{display:block;margin-top:8px;color:var(--dim)}
</style></head><body>
<h1>SYNTH CITY · SYNC</h1>
<div class="card" id="cBridge" hidden>
 <div id="ask"><p>Die Handy-App von <b class="org" id="org">?</b> möchte Projekte, Sounds und Samples mit diesem Gerät abgleichen.</p>
 <button class="go" id="allow">Erlauben</button><button id="deny">Abbrechen</button></div>
 <div id="wait">Warte auf die App …</div>
 <progress id="prB" max="1" value="0" hidden></progress>
 <div id="logB"></div>
</div>
<div class="card" id="cTool">
 <p><b>Ohne App-Verbindung</b>: alles vom Gerät als ZIP sichern (z. B. in die Dateien-App) oder ein ZIP einspielen (z. B. aus der Handy-App „Als ZIP sichern“).</p>
 <label><input type="checkbox" id="rec"> Aufnahmen (samples/rec) mitnehmen</label>
 <button class="go" id="zipDl">Alles als ZIP herunterladen</button>
 <label class="btn" style="color:var(--text)">ZIP einspielen<input type="file" id="zipIn" accept=".zip,application/zip" hidden></label>
 <progress id="prT" max="1" value="0" hidden></progress>
 <div id="log"></div>
 <p style="color:var(--dim);font-size:13px"><a href="/" style="color:var(--cy)">Dateien</a> · <a href="/screen" style="color:var(--cy)">Fernbedienung</a></p>
</div>
<script>
'use strict';
const $=id=>document.getElementById(id);
const enc=encodeURIComponent;
function log(el,t,cls){const d=document.createElement('div');if(cls)d.className=cls;d.textContent=t;$(el).appendChild(d);$(el).scrollTop=1e9;}
const ROOTS={projects:'.gbx',presets:'.gbp',samples:'.wav'};
async function manifest(rec){const r=await fetch('/api/manifest'+(rec?'?rec=1':''),{cache:'no-store'});if(!r.ok)throw new Error('Liste: '+r.status);return (await r.json()).files;}
async function getFile(r,p){const x=await fetch('/api/file?root='+r+'&path='+enc(p),{cache:'no-store'});if(!x.ok)throw new Error(p+': '+x.status);return await x.arrayBuffer();}
async function putFile(r,p,data){const x=await fetch('/api/upload?root='+r+'&path='+enc(p),{method:'PUT',body:data});if(!x.ok)throw new Error(p+': '+(await x.text()));}
// ---- ZIP (ohne Kompression schreiben; lesen auch mit Deflate)
const CRC=(()=>{const t=new Uint32Array(256);for(let n=0;n<256;n++){let c=n;for(let k=0;k<8;k++)c=c&1?0xedb88320^(c>>>1):c>>>1;t[n]=c>>>0;}return t;})();
function crc32(u){let c=0xffffffff;for(let i=0;i<u.length;i++)c=CRC[(c^u[i])&255]^(c>>>8);return(c^0xffffffff)>>>0;}
function zipBuild(list){const te=new TextEncoder(),parts=[],cen=[];let off=0;
 for(const f of list){const n=te.encode(f.name),d=new Uint8Array(f.data),c=crc32(d),h=new DataView(new ArrayBuffer(30));
  h.setUint32(0,0x04034b50,true);h.setUint16(4,20,true);h.setUint16(6,0x0800,true);h.setUint16(8,0,true);h.setUint16(12,0x21,true);h.setUint32(14,c,true);h.setUint32(18,d.length,true);h.setUint32(22,d.length,true);h.setUint16(26,n.length,true);
  parts.push(h.buffer,n,d);const ce=new DataView(new ArrayBuffer(46));ce.setUint32(0,0x02014b50,true);ce.setUint16(4,20,true);ce.setUint16(6,20,true);ce.setUint16(8,0x0800,true);ce.setUint16(14,0x21,true);
  ce.setUint32(16,c,true);ce.setUint32(20,d.length,true);ce.setUint32(24,d.length,true);ce.setUint16(28,n.length,true);ce.setUint32(42,off,true);cen.push(ce.buffer,n);off+=30+n.length+d.length;}
 let cl=0;cen.forEach(x=>cl+=x.byteLength);const e=new DataView(new ArrayBuffer(22));e.setUint32(0,0x06054b50,true);e.setUint16(8,list.length,true);e.setUint16(10,list.length,true);e.setUint32(12,cl,true);e.setUint32(16,off,true);
 return new Blob([...parts,...cen,e.buffer],{type:'application/zip'});}
async function zipRead(buf){const v=new DataView(buf),td=new TextDecoder();let e=-1;
 for(let i=buf.byteLength-22;i>=Math.max(0,buf.byteLength-65557);i--)if(v.getUint32(i,true)===0x06054b50){e=i;break;}
 if(e<0)throw new Error('kein ZIP');const n=v.getUint16(e+10,true);let p=v.getUint32(e+16,true);const out=[];
 for(let k=0;k<n;k++){if(v.getUint32(p,true)!==0x02014b50)throw new Error('ZIP beschädigt');
  const m=v.getUint16(p+10,true),cs=v.getUint32(p+20,true),nl=v.getUint16(p+28,true),xl=v.getUint16(p+30,true),cl=v.getUint16(p+32,true),lo=v.getUint32(p+42,true);
  const name=td.decode(new Uint8Array(buf,p+46,nl));p+=46+nl+xl+cl;if(name.endsWith('/'))continue;
  const ds=lo+30+v.getUint16(lo+26,true)+v.getUint16(lo+28,true);let data=buf.slice(ds,ds+cs);
  if(m===8){if(!window.DecompressionStream)throw new Error('Gepacktes ZIP: Browser zu alt');data=await new Response(new Blob([data]).stream().pipeThrough(new DecompressionStream('deflate-raw'))).arrayBuffer();}
  else if(m!==0)throw new Error(name+': Packverfahren '+m+' nicht unterstützt');
  out.push({name,data});}
 return out;}
function splitName(name){const i=name.indexOf('/');if(i<0)return null;const r=name.slice(0,i),p=name.slice(i+1);if(!ROOTS[r]||!p.toLowerCase().endsWith(ROOTS[r]))return null;return{r,p};}
// ---- Werkzeug: ZIP
$('zipDl').onclick=async()=>{const b=$('zipDl');b.disabled=true;$('prT').hidden=false;$('log').textContent='';
 try{const fs=await manifest($('rec').checked),list=[];let i=0;
  for(const f of fs){list.push({name:f.r+'/'+f.p,data:await getFile(f.r,f.p)});$('prT').value=++i/fs.length;}
  const blob=zipBuild(list),a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='synth-city-'+new Date().toISOString().slice(0,10)+'.zip';document.body.appendChild(a);a.click();
  setTimeout(()=>{URL.revokeObjectURL(a.href);a.remove();},60000);log('log',list.length+' Dateien ('+Math.round(blob.size/1048576)+' MB)','ok');}
 catch(e){log('log','Fehler: '+e.message,'bad');}b.disabled=false;};
$('zipIn').onchange=async e=>{const f=e.target.files[0];if(!f)return;$('prT').hidden=false;$('log').textContent='';
 try{const es=await zipRead(await f.arrayBuffer());let i=0,ok=0,skip=0;
  for(const x of es){const s=splitName(x.name);if(!s){skip++;continue;}await putFile(s.r,s.p,x.data);ok++;$('prT').value=++i/es.length;}
  log('log',ok+' Dateien eingespielt'+(skip?', '+skip+' übersprungen':''),'ok');}
 catch(err){log('log','Fehler: '+err.message,'bad');}e.target.value='';};
// ---- Bruecke fuer die Handy-App
let peer=null,peerOrigin='',pending=null;
function send(m,tr){if(peer)peer.postMessage(m,peerOrigin,tr||[]);}
if(window.opener){$('cBridge').hidden=false;$('ask').hidden=true;}
addEventListener('message',async e=>{
 const m=e.data;if(!m||typeof m!=='object')return;
 if(m.t==='hello'){
  if(!window.opener||e.source!==window.opener)return;   // nur das Fenster, das diese Seite geoeffnet hat
  if(peer){if(e.source===peer&&e.origin===peerOrigin&&m.again)send({t:'ready'});return;}
  if(pending)return;pending=e;$('cBridge').hidden=false;$('wait').hidden=true;$('ask').hidden=false;$('org').textContent=e.origin;return;}
 if(!peer||e.source!==peer||e.origin!==peerOrigin)return;
 try{
  if(m.t==='manifest'){log('logB','Liste des Geräts …');send({t:'manifest',files:await manifest(m.rec)});}
  else if(m.t==='plan'){const n=(m.put||[]).length+(m.get||[]).length;let i=0;$('prB').hidden=false;
   for(const f of m.put||[]){await putFile(f.r,f.p,f.data);$('prB').value=++i/n;}
   if((m.put||[]).length)log('logB',m.put.length+' Dateien aufs Gerät');
   // geholte Dateien in Haeppchen (~8 MB) schicken: grosse Bibliotheken sprengen sonst den Speicher (iPhone)
   let got=[],tr=[],sz=0,cnt=0;
   for(const f of m.get||[]){const d=await getFile(f.r,f.p);got.push({r:f.r,p:f.p,data:d});tr.push(d);sz+=d.byteLength;cnt++;$('prB').value=++i/n;
    if(sz>=8e6){send({t:'chunk',got,done:i,total:n},tr);got=[];tr=[];sz=0;}}
   if(cnt)log('logB',cnt+' Dateien vom Gerät');
   send({t:'result',got,files:await manifest(m.rec)},tr);if(m.last!==false)log('logB','Zurück zur App …');}
  else if(m.t==='done'){log('logB',m.text||'Fertig','ok');$('prB').value=1;setTimeout(()=>{try{window.close();}catch(x){}},2500);}
 }catch(err){log('logB','Fehler: '+err.message,'bad');send({t:'error',text:err.message});}
});
$('allow').onclick=()=>{if(!pending)return;peer=pending.source;peerOrigin=pending.origin;pending=null;$('ask').hidden=true;log('logB','Verbunden mit '+peerOrigin,'ok');send({t:'ready'});};
$('deny').onclick=()=>{if(pending){pending.source.postMessage({t:'denied'},pending.origin);pending=null;}$('ask').hidden=true;log('logB','Abgelehnt');};
</script></body></html>)HTML";
