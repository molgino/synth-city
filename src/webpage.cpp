// Upload-Seite (wird vom Mini-Webserver ausgeliefert). Umwandlung nach WAV passiert im Browser.
extern const char WEB_PAGE[];
const char WEB_PAGE[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SYNTH CITY</title>
<style>
:root{--bg:#080612;--panel:#141026;--panel2:#20183a;--text:#d6eeff;--dim:#8a80b0;--acc:#ff2e88;--cy:#00e0ff;--ok:#00ffaa;--bad:#ff4060;--lock:#ffc040}
*{box-sizing:border-box}body{margin:0;color:var(--text);font:15px/1.45 ui-monospace,Menlo,Consolas,monospace;background:linear-gradient(180deg,#080612 0%,#140a2a 60%,#2a0a3a 100%) fixed}
main{max-width:920px;margin:0 auto;padding:20px 16px 60px}
h1{font-size:30px;margin:0 0 4px;letter-spacing:.18em;color:#fff;text-shadow:-2px 0 var(--acc),2px 0 var(--cy),0 0 18px var(--acc)}
h2{font-size:14px;color:var(--acc);letter-spacing:.12em;margin:28px 0 10px;text-transform:uppercase}
.sub{color:var(--dim);margin:0 0 18px}a{color:var(--cy)}
.row{display:flex;flex-wrap:wrap;gap:8px 14px;align-items:center;margin:10px 0}
select,input[type=text],input[type=search]{background:var(--panel2);color:var(--text);border:1px solid #3a3a46;border-radius:8px;padding:6px 10px;font:inherit}
input[type=search]{flex:1;min-width:180px}
label{color:var(--dim)}label input{accent-color:var(--acc)}
button{background:var(--panel2);color:var(--text);border:1px solid #3a3a46;border-radius:8px;padding:5px 10px;font:inherit;cursor:pointer;white-space:nowrap}
button:hover{border-color:var(--acc)}button.del:hover{border-color:var(--bad);color:var(--bad)}button.pri{border-color:var(--acc);color:#fff}
button.ic{padding:4px 8px;min-width:34px}
.list{background:var(--panel);border-radius:12px;overflow:hidden;position:relative}
.list.over{outline:2px dashed var(--acc);outline-offset:-4px;background:#22102e}
.item{display:flex;align-items:center;gap:8px;padding:6px 10px;border-top:1px solid #26222e}
.item:first-child{border-top:0}.item .n{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.item .s{color:var(--dim);font-size:13px;white-space:nowrap}
.item.dir .n{color:var(--lock);cursor:pointer}.item.dir .n:hover{text-decoration:underline}
.item.up .n{color:var(--dim);cursor:pointer}
.item input[type=checkbox]{accent-color:var(--acc);width:16px;height:16px}
.item.sel{background:#2a1838}
.info{padding:6px 12px 10px 44px;color:var(--dim);font-size:13px;border-top:1px dashed #2e2840}
.crumb{display:flex;flex-wrap:wrap;gap:4px;align-items:center;margin:10px 0 8px}
.crumb a{cursor:pointer;color:var(--cy);text-decoration:none;padding:2px 6px;border-radius:6px;background:var(--panel2)}
.crumb a:last-child{color:#fff;background:var(--acc)}
.crumb span{color:var(--dim)}
#selbar{display:none;position:sticky;bottom:10px;margin-top:10px;background:var(--panel2);border:1px solid var(--acc);border-radius:12px;padding:8px 12px;gap:10px;align-items:center;box-shadow:0 0 20px #0008}
.job{display:flex;gap:10px;padding:5px 12px;font-size:13px}.job .n{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ok{color:var(--ok)}.bad{color:var(--bad)}.dim{color:var(--dim)}
#bar{height:4px;background:var(--panel2);border-radius:2px;overflow:hidden;margin-top:8px}#bar i{display:block;height:100%;width:0;background:var(--acc);transition:width .2s}
.hint{font-size:13px;color:var(--dim);margin:6px 2px}
@media(max-width:600px){.item{flex-wrap:wrap}.item .n{order:-1;flex:1 1 100%;white-space:normal;word-break:break-word}.item.dir .n,.item.up .n{order:0;flex:1}}
</style></head><body><main>
<h1>SYNTH <b>CITY</b></h1>
<p class="sub" id="info">verbinde …</p>
<p><a href="/screen" style="font-weight:700">▶ Bildschirm &amp; Fernbedienung</a> <span class="dim">– Live-Bild der Groovebox, bedienbar per Maus, Touch oder Tastatur</span></p>

<h2>Samples</h2>
<div class="row">
 <input type="search" id="q" placeholder="Samples suchen (alle Ordner) …">
 <button id="mk">+ Ordner</button>
 <button id="upf" class="pri">Dateien hochladen</button>
 <button id="upd2">Ordner hochladen</button>
 <input type="file" id="pick" multiple accept="audio/*,.wav,.mp3,.flac,.aif,.aiff,.ogg,.m4a" hidden>
 <input type="file" id="pickdir" webkitdirectory multiple hidden>
</div>
<div class="row">
 <label><input type="checkbox" id="conv" checked> in Mono/44,1 kHz umwandeln</label>
 <label><input type="checkbox" id="norm"> normalisieren</label>
</div>
<div class="crumb" id="crumb"></div>
<div id="samples" class="list"></div>
<p class="hint">Dateien oder ganze Ordner auf die Liste ziehen = Hochladen in den angezeigten Ordner (Unterordner bleiben erhalten). MP3, FLAC, AIFF, OGG … werden im Browser in WAV umgewandelt. Umbenennen/Verschieben: Projekte merken sich Samples per Name und finden verschobene Dateien danach nicht mehr.</p>
<div id="selbar"><b id="seln"></b><span style="flex:1"></span><button id="selmv">verschieben …</button><button id="seldel" class="del">löschen</button><button id="selnone">Auswahl aufheben</button></div>
<div id="bar"><i></i></div>
<div id="jobs" class="list" style="margin-top:8px"></div>

<h2>Projekte</h2>
<div class="row"><button id="pup" class="pri">Projekt (.gbx) hochladen</button><input type="file" id="ppick" accept=".gbx" multiple hidden>
<span class="dim">Name = Dateiname. Laden am Gerät unter PRJ → LADEN. Herunterladen = Sicherung.</span></div>
<div id="projects" class="list"></div>

<div id="upd" style="display:none">
<h2>Groovebox aktualisieren</h2>
<div class="row"><span>Installiert: <b id="ver">?</b></span><button id="uup">Update-Datei (.tgz) wählen</button><input type="file" id="upick" accept=".tgz" hidden>
<span id="ust" class="dim">Datei groovebox-os-update.tgz aus dem Release „nightly“</span></div>
</div>

<h2>Logs</h2>
<div class="row"><select id="logsel"><option>groovebox.log</option><option>groovebox.old.log</option><option>boot.txt</option><option>update.log</option><option>bench.txt</option><option>pc-modus.log</option><option>usb-diagnose</option></select>
<button id="logload">anzeigen</button><label><input type="checkbox" id="logauto"> automatisch aktualisieren</label><a id="logdl" download><button>herunterladen</button></a></div>
<pre id="logview" style="background:var(--panel);border-radius:12px;padding:12px;max-height:420px;overflow:auto;font-size:12px;white-space:pre-wrap;display:none"></pre>
</main>
<script>
const $=s=>document.querySelector(s);
const MAXDEPTH=8;
let cwd=localStorage.getItem('gbdir')||'',listing={dirs:[],files:[]},projects=[],sel=new Set(),query='',hits=null,infoOpen='';
const fmt=b=>b>1e9?(b/1e9).toFixed(1)+' GB':b>1e6?(b/1e6).toFixed(1)+' MB':Math.ceil(b/1e3)+' kB';
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const enc=encodeURIComponent;
// Namensteil bereinigen: Zeichen, die FAT nicht kennt, und Steuerzeichen raus; Umlaute bleiben
const cleanSeg=s=>s.normalize('NFC').replace(/[\\:*?"<>|\x00-\x1f\x7f\/]/g,'_').replace(/^[.\s]+/,'').replace(/\s+$/,'').slice(0,60)||'sample';
const join=(a,b)=>a?(b?a+'/'+b:a):b;
const depth=p=>p?p.split('/').length:0;
async function api(url,opt){const r=await fetch(url,opt);if(!r.ok)throw new Error(await r.text()||r.status);return r;}
async function refreshInfo(){
 try{const st=await (await fetch('/api/list')).json();projects=st.projects||[];
  $('#info').textContent=`${st.count} Samples · ${fmt(st.free)} frei auf der Karte`;
  if(st.os){$('#upd').style.display='';$('#ver').textContent=st.version||'?';}
  drawProjects();
 }catch(e){$('#info').textContent='keine Verbindung zum R36S';}
}
async function loadDir(d){
 try{listing=await (await api('/api/dir?root=samples&path='+enc(d))).json();cwd=d;}
 catch(e){if(d){return loadDir(d.includes('/')?d.slice(0,d.lastIndexOf('/')):'');}listing={dirs:[],files:[]};}
 try{localStorage.setItem('gbdir',cwd)}catch(e){}
 sel.clear();infoOpen='';draw();
}
function drawCrumb(){
 const parts=cwd?cwd.split('/'):[];let h=`<a data-cd="">samples</a>`,acc='';
 parts.forEach(p=>{acc=join(acc,p);h+=`<span>/</span><a data-cd="${esc(acc)}">${esc(p)}</a>`});
 $('#crumb').innerHTML=query?`<span>Suche „${esc(query)}“ – ${hits?hits.hits.length:0}${hits&&hits.more?'+':''} Treffer</span>`:h;
}
function fileRow(path,name,size,showDir){
 const k=esc(path),checked=sel.has(path);
 let h=`<div class="item${checked?' sel':''}"><input type="checkbox" data-sel="${k}"${checked?' checked':''}><button class="ic" data-play="${k}" title="anhören">▶</button>
  <span class="n" title="${k}">${esc(name)}${showDir?` <span class="dim">· ${esc(showDir)}</span>`:''}</span>${size!=null?`<span class="s">${fmt(size)}</span>`:''}
  <button class="ic" data-info="${k}" title="Infos">i</button><button class="ic" data-ren="${k}" title="umbenennen">✎</button>
  <a href="/api/file?root=samples&path=${enc(path)}" download="${esc(name)}"><button class="ic" title="herunterladen">⤓</button></a>
  <button class="ic del" data-del="${k}" title="löschen">✕</button></div>`;
 if(infoOpen===path)h+=`<div class="info" id="infobox">lese …</div>`;
 return h;
}
function draw(){
 drawCrumb();
 let h='';
 if(query){
  if(!hits)h='<div class="item dim">suche …</div>';
  else{hits.hits.forEach(p=>{const i=p.lastIndexOf('/');h+=fileRow(p,i<0?p:p.slice(i+1),null,i<0?'':p.slice(0,i))});if(!hits.hits.length)h='<div class="item dim">nichts gefunden</div>';}
 }else{
  if(cwd)h+=`<div class="item up"><span class="ic"></span><span class="n" data-cd="${esc(cwd.includes('/')?cwd.slice(0,cwd.lastIndexOf('/')):'')}">↰ ..</span></div>`;
  listing.dirs.sort((a,b)=>a.name.localeCompare(b.name,'de',{sensitivity:'base'})).forEach(d=>{const p=join(cwd,d.name),k=esc(p);
   h+=`<div class="item dir"><span style="width:16px">📁</span><span class="n" data-cd="${k}">${esc(d.name)}</span><span class="s">${d.n} ${d.n==1?'Eintrag':'Einträge'}</span>
   <button class="ic" data-ren="${k}" data-isdir="1" title="umbenennen/verschieben">✎</button><button class="ic del" data-rmdir="${k}" data-n="${d.n}" title="Ordner löschen">✕</button></div>`});
  listing.files.sort((a,b)=>a.name.localeCompare(b.name,'de',{numeric:true,sensitivity:'base'})).forEach(f=>{h+=fileRow(join(cwd,f.name),f.name,f.size)});
  if(!listing.dirs.length&&!listing.files.length)h+='<div class="item dim">Ordner ist leer – Dateien hierher ziehen</div>';
 }
 $('#samples').innerHTML=h;
 const n=sel.size;$('#selbar').style.display=n?'flex':'none';$('#seln').textContent=n+' ausgewählt';
 if(infoOpen)showInfo(infoOpen);
}
async function showInfo(p){
 try{const j=await (await api('/api/info?root=samples&path='+enc(p))).json();const b=$('#infobox');if(!b)return;
  b.innerHTML=j.ok?`${j.format} · ${j.ch==1?'Mono':j.ch==2?'Stereo':j.ch+' Kanäle'} · ${j.rate} Hz · ${j.bits} Bit · <b style="color:var(--text)">${j.sec.toFixed(2)} s</b>${j.sec>60?' <span class="bad">(länger als 60 s – wird gekürzt)</span>':''}`:'<span class="bad">kein lesbares WAV</span>';
 }catch(e){const b=$('#infobox');if(b)b.textContent='Fehler: '+e.message;}
}
function drawProjects(){
 $('#projects').innerHTML=projects.sort((a,b)=>a.path.localeCompare(b.path,'de',{sensitivity:'base'})).map(p=>{const k=esc(p.path),n=p.path.replace(/\.gbx$/i,'');
  return `<div class="item"><span class="n">${esc(n)}</span><span class="s">${fmt(p.size)}</span><a href="/api/file?root=projects&path=${enc(p.path)}" download="${k}"><button>herunterladen</button></a>
  <button class="ic" data-pren="${k}" title="umbenennen">✎</button><button class="ic del" data-pdel="${k}" title="löschen">✕</button></div>`}).join('')||'<div class="item dim">keine Projekte</div>';
}
let audio=null;
document.addEventListener('click',async e=>{
 const t=e.target.closest('[data-cd],[data-play],[data-del],[data-rmdir],[data-ren],[data-info],[data-pren],[data-pdel]');if(!t)return;const d=t.dataset;
 try{
  if(d.cd!==undefined){query='';$('#q').value='';hits=null;loadDir(d.cd);}
  else if(d.play){if(audio)audio.pause();audio=new Audio('/api/file?root=samples&path='+enc(d.play));audio.play();}
  else if(d.info){infoOpen=infoOpen===d.info?'':d.info;draw();}
  else if(d.del){if(confirm('Sample „'+d.del+'“ löschen?')){await api('/api/delete?root=samples&path='+enc(d.del),{method:'POST'});changed();}}
  else if(d.rmdir){if(confirm(`Ordner „${d.rmdir}“${+d.n?' mit allem Inhalt ('+d.n+' Einträge)':''} löschen?`)){await api('/api/rmdir?root=samples&path='+enc(d.rmdir),{method:'POST'});changed();}}
  else if(d.ren){const isDir=!!d.isdir,i=d.ren.lastIndexOf('/'),dir=i<0?'':d.ren.slice(0,i),name=i<0?d.ren:d.ren.slice(i+1);
   let to=prompt(`Neuer Pfad für ${isDir?'den Ordner':'das Sample'} (Ordner mit / trennen):`,d.ren);if(!to||to===d.ren)return;
   to=to.split('/').filter(x=>x).map(cleanSeg).join('/');if(!isDir&&!/\.wav$/i.test(to))to+='.wav';
   await api('/api/move?root=samples&path='+enc(d.ren)+'&to='+enc(to),{method:'POST'});changed();}
  else if(d.pdel){if(confirm('Projekt „'+d.pdel+'“ löschen?')){await api('/api/delete?root=projects&path='+enc(d.pdel),{method:'POST'});refreshInfo();}}
  else if(d.pren){let to=prompt('Neuer Projektname:',d.pren.replace(/\.gbx$/i,''));if(!to)return;to=cleanSeg(to.replace(/\//g,'_'))+'.gbx';
   await api('/api/move?root=projects&path='+enc(d.pren)+'&to='+enc(to),{method:'POST'});refreshInfo();}
 }catch(err){alert(err.message||err);}
});
document.addEventListener('change',e=>{const p=e.target.dataset&&e.target.dataset.sel;if(p===undefined)return;if(e.target.checked)sel.add(p);else sel.delete(p);
 e.target.closest('.item').classList.toggle('sel',e.target.checked);const n=sel.size;$('#selbar').style.display=n?'flex':'none';$('#seln').textContent=n+' ausgewählt';});
$('#selnone').onclick=()=>{sel.clear();draw();};
$('#seldel').onclick=async()=>{if(!confirm(sel.size+' Samples löschen?'))return;for(const p of [...sel]){try{await api('/api/delete?root=samples&path='+enc(p),{method:'POST'});}catch(e){}}changed();};
$('#selmv').onclick=async()=>{let to=prompt('Zielordner (z. B. drums/kicks, leer = Hauptordner):',cwd);if(to===null)return;
 to=to.split('/').filter(x=>x).map(cleanSeg).join('/');let bad=0;
 for(const p of [...sel]){const n=p.slice(p.lastIndexOf('/')+1),dst=join(to,n);if(dst===p)continue;try{await api('/api/move?root=samples&path='+enc(p)+'&to='+enc(dst),{method:'POST'});}catch(e){bad++;}}
 if(bad)alert(bad+' Dateien nicht verschoben (Ziel existiert schon?)');changed();};
function changed(){refreshInfo();if(query)search(query);else loadDir(cwd);}
$('#mk').onclick=async()=>{const n=prompt('Name des neuen Ordners in /samples/'+(cwd?cwd+'/':'')+':');if(!n)return;
 const p=join(cwd,n.split('/').filter(x=>x).map(cleanSeg).join('/'));if(depth(p)>MAXDEPTH){alert('Höchstens '+MAXDEPTH+' Ordnerebenen');return;}
 try{await api('/api/mkdir?root=samples&path='+enc(p),{method:'POST'});loadDir(p);}catch(e){alert(e.message);}};
let sTimer=0;
async function search(q){hits=null;draw();try{hits=await (await api('/api/search?root=samples&q='+enc(q))).json();}catch(e){hits={hits:[]};}if(query===q)draw();}
$('#q').oninput=e=>{clearTimeout(sTimer);query=e.target.value.trim();sTimer=setTimeout(()=>{if(query)search(query);else{hits=null;draw();}},250);};
// ---- Hochladen (Dateien, Ordner, Ziehen & Ablegen)
function wav(buf){
 const ch=buf.numberOfChannels,len=buf.length,rate=buf.sampleRate,out=new DataView(new ArrayBuffer(44+len*ch*2));
 const ws=(o,s)=>{for(let i=0;i<s.length;i++)out.setUint8(o+i,s.charCodeAt(i))};
 ws(0,'RIFF');out.setUint32(4,36+len*ch*2,true);ws(8,'WAVE');ws(12,'fmt ');out.setUint32(16,16,true);out.setUint16(20,1,true);
 out.setUint16(22,ch,true);out.setUint32(24,rate,true);out.setUint32(28,rate*ch*2,true);out.setUint16(32,ch*2,true);out.setUint16(34,16,true);
 ws(36,'data');out.setUint32(40,len*ch*2,true);
 const data=[];for(let c=0;c<ch;c++)data.push(buf.getChannelData(c));
 let peak=0;if($('#norm').checked)data.forEach(d=>{for(let i=0;i<len;i++)peak=Math.max(peak,Math.abs(d[i]))});
 const g=peak>0.0001?0.98/peak:1;let o=44;
 for(let i=0;i<len;i++)for(let c=0;c<ch;c++){let v=Math.max(-1,Math.min(1,data[c][i]*g));out.setInt16(o,v<0?v*32768:v*32767,true);o+=2}
 return new Blob([out],{type:'audio/wav'});
}
async function convert(file){
 const raw=await file.arrayBuffer();
 const ac=new (window.AudioContext||window.webkitAudioContext)();
 const dec=await ac.decodeAudioData(raw.slice(0));ac.close();
 const rate=44100,len=Math.min(Math.ceil(dec.duration*rate),rate*60);
 const off=new OfflineAudioContext(1,len,rate),src=off.createBufferSource();src.buffer=dec;src.connect(off.destination);src.start();
 return wav(await off.startRendering());
}
const AUDIO=/\.(wav|mp3|flac|aif|aiff|ogg|oga|m4a|aac|opus|webm)$/i;
// items: [{file, rel}] rel = Pfad relativ zum aktuellen Ordner (mit Unterordnern)
async function uploadItems(items){
 items=items.filter(it=>AUDIO.test(it.file.name));
 if(!items.length){alert('Keine Audiodateien gefunden');return;}
 const base=cwd;let done=0;$('#bar i').style.width='0';
 for(const it of items){
  const parts=it.rel.split('/').filter(x=>x),fname=parts.pop();
  const sub=parts.map(cleanSeg).join('/'),name=cleanSeg(fname.replace(/\.[^.]+$/,''))+'.wav',path=join(join(base,sub),name);
  const row=document.createElement('div');row.className='job';row.innerHTML=`<span class="n">${esc(join(sub,fname))}</span><span class="st dim">wandle um …</span>`;$('#jobs').prepend(row);
  const st=row.querySelector('.st');
  try{
   if(depth(path)-1>MAXDEPTH)throw new Error('zu viele Ordnerebenen (max. '+MAXDEPTH+')');
   const isWav=/\.wav$/i.test(fname);let blob=it.file;
   if(!isWav||$('#conv').checked)blob=await convert(it.file);
   st.textContent='lade hoch …';
   await api('/api/upload?root=samples&path='+enc(path),{method:'PUT',body:blob});
   st.textContent='✓ '+fmt(blob.size);st.className='st ok';
  }catch(e){st.textContent='✗ '+(e.message||e);st.className='st bad';}
  done++;$('#bar i').style.width=(done/items.length*100)+'%';
 }
 changed();
}
$('#upf').onclick=()=>$('#pick').click();$('#upd2').onclick=()=>$('#pickdir').click();
$('#pick').onchange=e=>{uploadItems([...e.target.files].map(f=>({file:f,rel:f.name})));e.target.value='';};
$('#pickdir').onchange=e=>{uploadItems([...e.target.files].map(f=>({file:f,rel:f.webkitRelativePath||f.name})));e.target.value='';};
function readEntry(en,prefix,out){return new Promise(res=>{
 if(en.isFile)en.file(f=>{out.push({file:f,rel:prefix+f.name});res();},()=>res());
 else if(en.isDirectory){const rd=en.createReader(),all=[];const next=()=>rd.readEntries(async es=>{if(es.length){all.push(...es);next();}else{for(const c of all)await readEntry(c,prefix+en.name+'/',out);res();}},()=>res());next();}
 else res();});}
const zone=$('#samples');
['dragenter','dragover'].forEach(t=>zone.addEventListener(t,e=>{e.preventDefault();zone.classList.add('over')}));
['dragleave','drop'].forEach(t=>zone.addEventListener(t,e=>{e.preventDefault();zone.classList.remove('over')}));
zone.addEventListener('drop',async e=>{const its=[...(e.dataTransfer.items||[])],out=[];
 if(its.length&&its[0].webkitGetAsEntry){const ens=its.map(i=>i.webkitGetAsEntry()).filter(x=>x);for(const en of ens)await readEntry(en,'',out);}
 else [...e.dataTransfer.files].forEach(f=>out.push({file:f,rel:f.name}));
 if(query){query='';$('#q').value='';hits=null;}
 uploadItems(out);});
// ---- Projekte
$('#pup').onclick=()=>$('#ppick').click();
$('#ppick').onchange=async e=>{for(const f of e.target.files){const n=cleanSeg(f.name.replace(/\.gbx$/i,'').replace(/\//g,'_'))+'.gbx';
 if(projects.some(p=>p.path.toLowerCase()===n.toLowerCase())&&!confirm('Projekt „'+n+'“ gibt es schon – überschreiben?'))continue;
 try{await api('/api/upload?root=projects&path='+enc(n),{method:'PUT',body:f});}catch(err){alert(n+': '+err.message);}}
 e.target.value='';refreshInfo();};
// ---- Update, Logs
$('#uup').onclick=()=>$('#upick').click();
$('#upick').onchange=async e=>{const f=e.target.files[0];if(!f)return;const st=$('#ust');st.className='';st.textContent='lade hoch …';
 try{const r=await fetch('/api/update',{method:'PUT',body:f});const t=await r.text();if(!r.ok)throw new Error(t);
  st.className='ok';st.textContent='✓ übertragen – Groovebox startet neu und installiert (ca. 15 s)';setTimeout(refreshInfo,15000);}
 catch(err){st.className='bad';st.textContent='✗ '+(err.message||err);}e.target.value='';};
async function loadLog(){const n=$('#logsel').value,v=$('#logview');v.style.display='';$('#logdl').href='/api/log?name='+n;$('#logdl').download=n;
 try{const r=await fetch('/api/log?name='+enc(n));v.textContent=await r.text();v.scrollTop=v.scrollHeight;}catch(e){v.textContent='keine Verbindung';}}
$('#logload').onclick=loadLog;$('#logsel').onchange=loadLog;setInterval(()=>{if($('#logauto').checked)loadLog()},3000);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)changed();});
refreshInfo();loadDir(cwd);
</script></body></html>)HTML";

// Bildschirm-Spiegelung + Fernbedienung (http://<ip>/screen)
extern const char WEB_SCREEN[];
const char WEB_SCREEN[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes"><meta name="mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#080612"><meta name="apple-mobile-web-app-status-bar-style" content="black-translucent"><meta name="apple-mobile-web-app-title" content="SYNTH CITY">
<link rel="manifest" href="/manifest.json"><link rel="apple-touch-icon" href="/apple-touch-icon.png"><link rel="icon" href="/icon-192.png">
<title>SYNTH CITY – Fernbedienung</title>
<style>
:root{--bg:#0e0f12;--panel:#1e2129;--panel2:#30333f;--line:#464b58;--text:#e4e8ee;--dim:#8c94a2;--acc:#ff3c8c;--cy:#68c8e0;--gr:#40d696;--b:clamp(44px,7vmin,64px)}
*{box-sizing:border-box;-webkit-user-select:none;user-select:none;-webkit-touch-callout:none;-webkit-tap-highlight-color:transparent}
html,body{margin:0;height:100%;background:linear-gradient(180deg,#080612,#140a2a 60%,#2a0a3a) fixed;color:var(--text);font:14px ui-monospace,Menlo,Consolas,monospace;overflow:hidden}
#app{height:100%;display:flex;flex-direction:column;padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)}
header{display:flex;align-items:center;gap:10px;padding:6px 10px;flex-wrap:wrap}
h1{font-size:16px;margin:0;letter-spacing:.18em;color:#fff;text-shadow:-2px 0 var(--acc),2px 0 var(--cy)}
#stat{color:var(--dim);font-size:12px}#dot{width:9px;height:9px;border-radius:50%;background:#555;display:inline-block}
header .sp{flex:1}a{color:var(--cy)}
.hb.on{background:var(--acc);border-color:var(--acc);color:#fff}
.hb{background:var(--panel2);color:var(--text);border:1px solid var(--line);border-radius:8px;font:inherit;font-size:12px;padding:5px 10px;cursor:pointer}
#stage{flex:1;min-height:0;display:grid;grid-template-columns:1fr;grid-template-rows:minmax(0,1fr) auto;gap:8px;padding:0 10px 10px}
#scr{min-height:0;min-width:0;overflow:hidden;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px}
canvas{display:block;flex:none;width:320px;height:240px;image-rendering:pixelated;image-rendering:crisp-edges;background:#000;border-radius:6px;box-shadow:0 0 0 2px #000,0 0 26px #ff2e8840;outline:none}
#ctl{display:grid;grid-template-columns:1fr auto 1fr;align-items:center;gap:8px;touch-action:none}
.side{display:flex;flex-direction:column;align-items:center;gap:10px}
.shd{display:flex;gap:6px}
.btn{width:calc(var(--b)*1.25);height:calc(var(--b)*.72);display:flex;align-items:center;justify-content:center;background:var(--panel2);border:1px solid var(--line);border-radius:10px;font-weight:700;color:var(--text);touch-action:none;cursor:pointer;transition:background .05s}
.btn.on{background:var(--acc);border-color:var(--acc);color:#fff;box-shadow:0 0 16px var(--acc)}
.dpad{position:relative;width:calc(var(--b)*2.9);height:calc(var(--b)*2.9);touch-action:none}
.dpad i{position:absolute;width:calc(var(--b)*.98);height:calc(var(--b)*.98);background:var(--panel2);border:1px solid var(--line);border-radius:10px;display:flex;align-items:center;justify-content:center;color:var(--dim);font-style:normal;font-size:18px}
.dpad i.on{background:var(--acc);border-color:var(--acc);color:#fff;box-shadow:0 0 16px var(--acc)}
.dpad .u{left:calc(50% - var(--b)*.49);top:0}.dpad .d{left:calc(50% - var(--b)*.49);bottom:0}
.dpad .l{top:calc(50% - var(--b)*.49);left:0}.dpad .r{top:calc(50% - var(--b)*.49);right:0}
.dpad .c{left:calc(50% - var(--b)*.3);top:calc(50% - var(--b)*.3);width:calc(var(--b)*.6);height:calc(var(--b)*.6);border-radius:50%;background:var(--panel)}
.face{position:relative;width:calc(var(--b)*2.9);height:calc(var(--b)*2.9);touch-action:none}
.face .btn{position:absolute;width:var(--b);height:var(--b);border-radius:50%;font-size:18px}
.face .x{left:calc(50% - var(--b)/2);top:0}.face .b{left:calc(50% - var(--b)/2);bottom:0}
.face .y{top:calc(50% - var(--b)/2);left:0}.face .a{top:calc(50% - var(--b)/2);right:0}
.face .x,.face .y{color:var(--cy)}
.mid{display:flex;flex-direction:column;align-items:center;gap:8px}
.mid .row{display:flex;gap:8px}
.mid .btn{width:calc(var(--b)*1.5);height:calc(var(--b)*.6);font-size:12px;border-radius:20px}
.ms{display:none}
.stk{display:none;grid-template-columns:repeat(3,calc(var(--b)*.6));grid-template-rows:repeat(3,calc(var(--b)*.6));gap:3px;touch-action:none}
.stk .btn{width:auto;height:auto;font-size:11px;border-radius:8px}
body.sticks .stk{display:grid}
#help{position:fixed;inset:0;background:#080612ee;display:none;align-items:center;justify-content:center;z-index:9;padding:20px}
#help.show{display:flex}#help div{max-width:560px;background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:18px 20px;line-height:1.9}
kbd{background:var(--panel2);border:1px solid var(--line);border-radius:5px;padding:1px 6px;color:var(--cy)}
/* Querformat (Handy quer, Tablet, PC-Fenster breit): Bild in der Mitte, Bedienung links und rechts */
@media (orientation:landscape) and (min-aspect-ratio:13/10){
 #stage{grid-template-columns:auto minmax(0,1fr) auto;grid-template-rows:minmax(0,1fr);align-items:center}
 #ctl{display:contents}
 .lside{grid-column:1;grid-row:1}.rside{grid-column:3;grid-row:1}
 #scr{grid-column:2;grid-row:1;height:100%;width:100%}
 .mid{flex-direction:row}
 /* Das Bild nimmt 4:3 der Hoehe, also 133,33vh Breite. Was uebrig bleibt, teilen sich die beiden
    Balken; daraus die Knopfgroesse (Steuerkreuz = 2,9 x --b), mindestens 40 px fuer den Finger. */
 :root{--b:clamp(40px,calc((100vw - 133.33vh - 36px)/5.8),72px)}
}
@media (max-height:420px){header{padding:2px 8px}h1{font-size:12px}#stat{font-size:10px}.hb{padding:2px 8px}}
#cvw{position:relative;flex:none;touch-action:none;overflow:hidden;border-radius:6px}
#cv{transform-origin:0 0}
#zl{min-width:46px;text-align:center}
#lost{position:absolute;inset:0;display:none;align-items:center;justify-content:center;text-align:center;background:rgba(8,4,18,.78);color:#fff;font-size:15px;line-height:1.6;z-index:5;backdrop-filter:grayscale(1)}#lost.on{display:flex}#lost b{color:#ff4fa3;font-size:18px}#lostT{color:var(--dim);font-size:12px}#lost button{margin-top:10px;padding:6px 16px;border-radius:6px;border:1px solid var(--cy);background:transparent;color:var(--cy);font:inherit}.spin{width:34px;height:34px;margin:0 auto 8px;border-radius:50%;border:3px solid rgba(255,255,255,.15);border-top-color:var(--cy);animation:spin 1s linear infinite}@keyframes spin{to{transform:rotate(360deg)}}.rip{position:absolute;width:44px;height:44px;margin:-22px 0 0 -22px;border-radius:50%;border:2px solid var(--cy);pointer-events:none;animation:rip .45s ease-out forwards}
.rip.hold{border-color:var(--acc);animation:none;opacity:.8}
@keyframes rip{from{transform:scale(.4);opacity:1}to{transform:scale(1.6);opacity:0}}
/* Bild-Vollbild: nur das Bild, so gross wie moeglich, schwebende Mini-Leiste */
body.full header,body.full #midL{display:none !important}
body.full #app{padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)}
body.full #stage{padding:0;gap:0;grid-template-columns:1fr !important;grid-template-rows:minmax(0,1fr) !important}
body.full #scr{grid-column:1 !important;grid-row:1 !important;width:100%;height:100%}
body.full #ctl{display:none !important}
/* Querformat-Vollbild: das Bild ist 4:3, der Schirm viel breiter - die Balken links und rechts, die dabei
   ohnehin frei bleiben, tragen die Bedienung. So ist der Schirm voll genutzt und die Finger liegen
   nicht auf dem Bild. "Tasten" (Knopf in der Leiste) blendet sie weg, dann ist nur das Bild da. */
@media (orientation:landscape) and (min-aspect-ratio:13/10){
 body.full:not(.nopad) #ctl{display:contents !important}
 body.full:not(.nopad) #stage{grid-template-columns:auto minmax(0,1fr) auto !important;grid-template-rows:minmax(0,1fr) !important;align-items:center}
 body.full:not(.nopad) #scr{grid-column:2 !important}
 body.full .side{gap:max(8px,2vh)}
 /* Mit sichtbarer Bedienung gehoert die Leiste nach unten in den Balken - oben wuerde sie die
    Ansichten-Reiter im Bild verdecken. Knoepfe etwas groesser, sie werden mit dem Daumen getroffen. */
 /* SELECT und START als volle Knoepfe in die Balken: linker und rechter Daumen, wie am Geraet.
    Die Leiste behaelt nur noch die Schalter und steht schmal gestapelt am Rand, damit sie
    nicht ins Bild ragt (der Balken ist nur halb so breit wie die Leiste in einer Zeile). */
 body.full:not(.nopad) .ms{display:flex;width:calc(var(--b)*2.3);height:calc(var(--b)*.72);font-size:12px;border-radius:20px}
 body.full:not(.nopad) #fbar{flex-direction:column;opacity:.8}
 body.full:not(.nopad) #fbar [data-b],body.full:not(.nopad) #fSide{display:none}
 body.full:not(.nopad) #fbar .btn{height:32px;font-size:11px;padding:0 10px}
}
body.full canvas,body.full #cvw{border-radius:0}body.full canvas{box-shadow:none}
#fbar{display:none;position:fixed;right:max(6px,env(safe-area-inset-right));top:max(6px,env(safe-area-inset-top));z-index:8;gap:6px;opacity:.55;touch-action:none}
body.full #fbar{display:flex}#fbar:hover,#fbar:active{opacity:1}
#fbar .btn{width:auto;height:30px;padding:0 10px;font-size:11px;border-radius:15px;background:#20183acc}
body.full.fleft #fbar{right:auto;left:max(6px,env(safe-area-inset-left))}
/* Die Leiste darf NIE ueber dem Bild liegen: oben rechts sitzen dort die Ansichten-Reiter, die waeren
   dann nicht antippbar. fit() legt sie in den freien Balken neben das Bild; ist dort kein Platz
   (4:3-Schirm), bleibt nur ein kleiner Griff, der sie auf Tap fuer ein paar Sekunden ausfaehrt. */
#fbar.mini .btn{display:none}
#fbar.mini{width:30px;height:30px;border-radius:15px;background:#20283acc;border:1px solid var(--line);
 align-items:center;justify-content:center;opacity:.5}
#fbar.mini::after{content:'···';color:var(--dim);font-size:15px;line-height:1}
/* ---- Touch-Oberflaeche: eigenes UI statt des gespiegelten 4:3-Bildes. Fuellt jede Aufloesung,
   die Flaechen sind so gross, wie der Schirm hergibt, und nichts ist an 640x480 gebunden. */
#tui{display:none;position:fixed;inset:0;z-index:6;background:var(--bg);
 padding:max(6px,env(safe-area-inset-top)) max(6px,env(safe-area-inset-right)) max(6px,env(safe-area-inset-bottom)) max(6px,env(safe-area-inset-left));
 grid-template-rows:auto minmax(0,1fr) auto;gap:6px;touch-action:manipulation;user-select:none}
body.tui #tui{display:grid}
body.tui #app{visibility:hidden}
#tTop,#tBot{display:flex;align-items:center;gap:6px;flex-wrap:wrap}
#tTop .tb,#tBot .tb{min-width:44px;min-height:44px;display:flex;align-items:center;justify-content:center;
 background:var(--panel);border:1px solid var(--line);border-radius:10px;color:var(--text);font:inherit;font-size:13px;padding:0 12px}
#tTop .tb:active,#tBot .tb:active{background:var(--panel2)}
#tModes{display:flex;gap:4px}
/* Mixer: 8 Spalten, Zeilen wie die MIX-Seite am Geraet (VOL PAN S.A S.B S.C CMP) */
#tMix{display:none;grid-row:2;grid-template-columns:repeat(8,1fr);gap:4px;min-height:0}
body.mix #tMix{display:grid}
body.mix #tMain{display:none}
.mxc{display:flex;flex-direction:column;gap:3px;min-width:0;min-height:0;background:var(--panel);border-radius:8px;padding:3px}
.mxc.sel{outline:1px solid var(--text)}
.mxh{font-size:11px;text-align:center;color:var(--dim);overflow:hidden;white-space:nowrap;min-height:22px;
 display:flex;align-items:center;justify-content:center;border-radius:5px;background:var(--panel2)}
.mxb{display:flex;gap:3px}
.mxb div{flex:1;min-height:26px;display:flex;align-items:center;justify-content:center;border-radius:5px;
 background:var(--off);color:var(--dim);font-size:11px}
.mxb div.on{background:var(--acc);color:var(--bg)}
.mxb div.so.on{background:var(--cy)}
.mxf{position:relative;flex:1;min-height:26px;border-radius:5px;background:var(--off);overflow:hidden;touch-action:none}
.mxf i{position:absolute;left:0;right:0;bottom:0;background:var(--acc);opacity:.55}
.mxf b{position:absolute;left:3px;top:2px;font-size:10px;font-weight:400;color:var(--dim)}
.mxf u{position:absolute;right:3px;top:2px;font-size:10px;text-decoration:none;color:var(--text)}
.mxf.sw{display:flex;align-items:center;justify-content:center}
@media (orientation:portrait){#tMix{grid-template-columns:repeat(4,1fr)}}
/* Song: eine Zeile je Song-Zeile, darin die acht Spuren als Zellen */
#tSong{display:none;grid-row:2;grid-template-rows:auto minmax(0,1fr);gap:6px;min-height:0}
body.song #tSong{display:grid}
body.song #tMain{display:none}
#tSgBar{display:flex;gap:6px;align-items:center;flex-wrap:wrap}
#tSgL{overflow-y:auto;overscroll-behavior:contain;display:flex;flex-direction:column;gap:4px}
.sgr{display:flex;gap:3px;align-items:stretch;background:var(--panel);border-radius:8px;padding:3px;border:1px solid var(--line)}
.sgr.now{border-color:var(--cy)}
.sgr>.nr{min-width:34px;display:flex;align-items:center;justify-content:center;font-size:12px;color:var(--dim);border-radius:5px;background:var(--panel2)}
.sgr>.cs{flex:1;display:grid;grid-template-columns:repeat(8,1fr);gap:3px;min-width:0}
.sgr .cl{min-height:40px;display:flex;align-items:center;justify-content:center;border-radius:5px;
 background:var(--off);font-size:11px;color:var(--text);overflow:hidden}
.sgr .cl.off{color:var(--dim);background:var(--bg)}
.sgr>.rp{display:flex;gap:2px;align-items:stretch}
.sgr>.rp div{min-width:30px;display:flex;align-items:center;justify-content:center;border-radius:5px;background:var(--off);font-size:12px}
.sgr>.rp div.v{min-width:34px;background:var(--panel2);color:var(--cy)}
.sgr>.del{min-width:34px;display:flex;align-items:center;justify-content:center;border-radius:5px;background:var(--off);color:var(--dim)}
@media (orientation:portrait){.sgr .cl{min-height:34px;font-size:10px}.sgr>.nr{min-width:26px}}
#tModes .tm{padding:0 10px}
#tModes .tm.on{background:var(--panel2);border-color:var(--text);color:var(--cy)}
#tPlay{font-size:20px;min-width:62px}
#tPlay.on{background:var(--gr);color:#06240f;border-color:var(--gr)}
#tPat{font-size:18px;font-weight:bold;color:var(--cy);min-width:62px;text-align:center}
#tBpm{color:var(--dim);font-size:13px}
#tMsg{color:var(--acc);font-size:12px;flex:1;text-align:right;overflow:hidden;white-space:nowrap}
#tMain{display:grid;grid-template-columns:minmax(96px,18%) minmax(0,1fr);gap:6px;min-height:0}
#tTracks{display:grid;grid-auto-rows:minmax(0,1fr);gap:4px;min-height:0;overflow-y:auto;overscroll-behavior:contain;scrollbar-width:none}
.ttr{display:flex;align-items:center;gap:4px;background:var(--panel);border:1px solid var(--line);
 border-left-width:5px;border-radius:9px;padding:0 6px;overflow:hidden;min-height:0}
.ttr.sel{background:var(--panel2);border-color:var(--text)}
.ttr .tn{flex:1;font-size:12px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
.ttr .tm{width:30px;min-height:30px;display:flex;align-items:center;justify-content:center;border-radius:7px;
 background:var(--panel2);color:var(--dim);font-size:11px}
.ttr .tm.on{background:var(--acc);color:#fff}
.ttr.play .tn{color:var(--text)}
#tGrid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));grid-template-rows:repeat(4,minmax(0,1fr));gap:5px;min-height:0}
/* quer: 8x2 wie die Step-Leiste am Geraet (nutzt die Breite), hoch: 4x4 wie ein Pad-Raster */
@media (orientation:landscape) and (min-aspect-ratio:13/10){
 #tGrid{grid-template-columns:repeat(8,minmax(0,1fr));grid-template-rows:repeat(2,minmax(0,1fr))}
 #tTop .tb,#tBot .tb{min-height:38px}}
.tp{position:relative;touch-action:none;border-radius:10px;background:var(--panel);border:1px solid var(--line);min-height:0;
 display:flex;align-items:flex-end;justify-content:flex-start;padding:4px 6px;font-size:11px;color:var(--dim)}
.tp.on{background:var(--acc);border-color:transparent;color:#fff}
.tp.tl{background:var(--panel2);box-shadow:inset 0 0 0 2px var(--acc)}
.tp.now{outline:3px solid var(--text);outline-offset:-3px}
.tp.beat::after{content:'';position:absolute;top:5px;right:6px;width:5px;height:5px;border-radius:50%;background:var(--line)}
@media (orientation:portrait){#tMain{grid-template-columns:minmax(0,1fr);grid-template-rows:auto minmax(0,1fr)}
 #tTracks{grid-auto-rows:auto;grid-template-columns:repeat(4,1fr)}
 .ttr{min-height:42px;padding:0 4px}
 .ttr .tn{font-size:11px}.ttr .tnr{display:none}}   /* hochkant ist kein Platz fuer die Nummer: die Farbe sagt es */
#tBot:empty,#tBot.hide{display:none}
/* Klang schrauben: Gruppenleiste zum Wischen + Regler als breite Zeilen (waagerecht ziehen = Wert) */
#tSnd{display:none;grid-template-rows:auto minmax(0,1fr);gap:6px;min-height:0}
body.tui.snd #tSnd{display:grid}
body.tui.snd #tGrid{display:none}
#tGrps{display:flex;gap:6px;overflow-x:auto;overflow-y:hidden;padding-bottom:2px;scrollbar-width:none}
#tGrps::-webkit-scrollbar{display:none}
#tGrps .tg{flex:none;min-height:38px;display:flex;align-items:center;padding:0 14px;border-radius:10px;
 background:var(--panel);border:1px solid var(--line);color:var(--dim);font-size:12px;white-space:nowrap}
#tGrps .tg.on{background:var(--panel2);color:var(--text);border-color:var(--text)}
#tPar{display:grid;grid-auto-rows:minmax(52px,1fr);gap:5px;min-height:0;overflow-y:auto;overscroll-behavior:contain;
 -webkit-overflow-scrolling:touch;scrollbar-width:thin}
#tPar.many{grid-auto-rows:minmax(52px,auto)}   /* mehr Regler als Platz: feste Hoehe, dafuer scrollbar */
.tr{position:relative;display:flex;align-items:center;gap:8px;padding:0 12px;border-radius:10px;
 background:var(--panel);border:1px solid var(--line);overflow:hidden;touch-action:pan-y}
.tr .bar{position:absolute;left:0;top:0;bottom:0;background:var(--panel2);pointer-events:none}
.tr .nm{position:relative;flex:1;font-size:13px;color:var(--text)}
.tr .vl{position:relative;font-size:15px;color:var(--cy);font-weight:bold;min-width:68px;text-align:right}
.tr.lk .vl{color:var(--lock,#ffba40)}
.tr.act{border-color:var(--text)}
/* P-Lock: Pad lange druecken haelt den Step fest, danach sind alle Reglerwerte Locks (gelb) */
#tLock{display:none;align-items:center;gap:8px}
body.tui.hold #tLock{display:flex}
#tLock .tb{background:var(--lk,#2a2317);border-color:#ffba40;color:#ffba40}
.tp.hold{outline:3px solid #ffba40;outline-offset:-3px}
body.tui.hold .tr .vl{color:#ffba40}
/* Klaviatur: haengt ein Step im P-Lock, schreibt die Taste dessen Note - sonst hoert man nur vor */
#tKeys{display:none;grid-template-rows:auto minmax(0,1fr);gap:6px;min-height:0}
body.tui.keys #tKeys{display:grid}
body.tui.keys #tGrid,body.tui.keys #tSnd{display:none}
#tOct{display:flex;align-items:center;gap:8px}
#tKb{position:relative;min-height:0;touch-action:none;display:flex;gap:3px}
#tKb .wk{flex:1;position:relative;border-radius:0 0 8px 8px;background:#e8ecf2;border:1px solid #2a2d35;
 display:flex;align-items:flex-end;justify-content:center;padding-bottom:6px;color:#2a2d35;font-size:11px}
#tKb .wk.on{background:var(--cy);color:#06242c}
#tKb .wk.cur{box-shadow:inset 0 0 0 3px var(--acc)}
#tKb .bk{position:absolute;top:0;height:62%;border-radius:0 0 6px 6px;background:#1a1d24;border:1px solid #000;
 z-index:2;display:flex;align-items:flex-end;justify-content:center;padding-bottom:4px;color:#aeb6c2;font-size:10px}
#tKb .bk.on{background:var(--cy);color:#06242c}
#tKb .bk.cur{box-shadow:inset 0 0 0 3px var(--acc)}
/* Auswahl (ENGINE, Filtertyp, Bus-Effekt ...): auf 15 Engines mit dem Finger zu zielen geht nicht
   mit einem Schieber - antippen oeffnet eine Liste mit grossen Zeilen. */
.tr.sel .vl::after{content:' ›';color:var(--dim)}
/* Der Picker war das einzige Element ohne Safe-Area: im Querformat lagen Kopfzeile und ABBRECHEN
   auf dem iPhone unter der Notch und waren nicht zu treffen (gemeldet "obere Elemente nicht bedienbar"). */
#tPick{display:none;position:fixed;inset:0;z-index:9;background:#0e0f12ee;
 padding:max(10px,env(safe-area-inset-top)) max(10px,env(safe-area-inset-right)) max(10px,env(safe-area-inset-bottom)) max(10px,env(safe-area-inset-left));
 grid-template-rows:auto minmax(0,1fr);gap:10px}
#tPickH,#tShH{display:flex;align-items:center;gap:10px;min-height:44px}
/* Projekt-Blatt: gleicher Bau wie der Picker (Safe-Area, 44-px-Flaechen) */
#tSheet{display:none;position:fixed;inset:0;z-index:9;background:#0e0f12f2;
 padding:max(10px,env(safe-area-inset-top)) max(10px,env(safe-area-inset-right)) max(10px,env(safe-area-inset-bottom)) max(10px,env(safe-area-inset-left));
 grid-template-rows:auto minmax(0,1fr);gap:10px}
body.sheet #tSheet{display:grid}
#tSheet .tb{min-width:44px;min-height:44px;display:flex;align-items:center;justify-content:center;
 border-radius:10px;background:var(--panel);border:1px solid var(--off2);color:var(--text);padding:0 14px;font:inherit}
#tShT{font-size:15px;color:var(--cy)}
#tShB{overflow-y:auto;overscroll-behavior:contain;display:flex;flex-direction:column;gap:8px}
#tShB .row{display:flex;gap:8px;flex-wrap:wrap}
#tShB .big{flex:1;min-height:52px;display:flex;align-items:center;justify-content:center;border-radius:10px;
 background:var(--panel);border:1px solid var(--off2);color:var(--text);font-size:14px}
#tShB .big.go{background:var(--acc);border-color:var(--acc);color:var(--bg)}
#tShB .lbl{color:var(--dim);font-size:12px}
#tShB .pl{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:6px}
#tShB .pl div{min-height:48px;display:flex;align-items:center;padding:0 12px;border-radius:9px;
 background:var(--panel);border:1px solid var(--line);overflow:hidden;white-space:nowrap;text-overflow:ellipsis}
#tShB .pl div.on{border-color:var(--text);color:var(--cy)}
#tPick .tb{min-width:44px;min-height:44px;display:flex;align-items:center;justify-content:center;
 border-radius:10px;background:var(--panel);border:1px solid var(--off2);color:var(--text);padding:0 14px;font:inherit}
body.pick #tPick{display:grid}
#tPickT{font-size:15px;color:var(--cy)}
#tPickL.wide{grid-template-columns:repeat(auto-fill,minmax(220px,1fr))}
#tPickL.wide .pk{justify-content:flex-start;font-size:13px;padding:0 12px;overflow:hidden;white-space:nowrap;text-overflow:ellipsis;display:block;line-height:52px}
#tPickL{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));grid-auto-rows:minmax(52px,auto);
 gap:8px;overflow-y:auto;overscroll-behavior:contain;align-content:start}
#tPickL .pk{display:flex;align-items:center;justify-content:center;border-radius:10px;background:var(--panel);
 border:1px solid var(--line);color:var(--text);font-size:14px;padding:0 8px;text-align:center}
#tPickL .pk.on{background:var(--panel2);border-color:var(--text);color:var(--cy)}
/* schmal (Handy hochkant): die Kopfzeile darf nicht umbrechen - Meldung weg, Schrift kleiner */
@media (max-width:560px){#tMsg{display:none}#tBpm{font-size:11px}#tTop .tb{padding:0 9px;font-size:12px}
 #tPat{font-size:16px;min-width:48px}#tPlay{min-width:52px}}
/* Solange kein Modell da ist, bleibt die Flaeche sonst einfach leer - dann weiss niemand, was los ist */
#tWait{display:none;position:absolute;inset:54px 6px 6px 6px;align-items:center;justify-content:center;
 text-align:center;color:var(--dim);font-size:14px;line-height:1.7;padding:16px}
body.tui.twait #tWait{display:flex}
body.tui.twait #tMain{visibility:hidden}
body.nopad #ctl{display:none}
body.nopad #stage{grid-template-columns:1fr !important}
body.nopad #scr{grid-column:1 !important}
</style></head><body><div id="app">
<header><span id="dot"></span><h1>SYNTH CITY</h1><span id="stat">…</span><span class="sp"></span>
<button class="hb" id="bAud" title="Ton im Browser">🔈 Ton</button><input id="vol" type="range" min="0" max="100" value="80" style="width:70px;display:none"><button class="hb" id="zo" title="Verkleinern (−)">−</button><button class="hb" id="zl" title="Zoom zurücksetzen (0)">100%</button><button class="hb" id="zi" title="Vergrößern (+)">+</button><button class="hb" id="bPad">Tasten</button><button class="hb" id="bSt">Sticks</button><button class="hb" id="bHelp">?</button><button class="hb" id="bTui" title="Touch-Oberfläche (für Finger ausgelegt)">Touch</button><button class="hb" id="bFull" title="Nur das Bild, so groß wie möglich (V)">Vollbild</button><button class="hb" id="bFs" title="Browser-Vollbild">⛶</button><a href="/" class="hb" style="text-decoration:none">Dateien</a></header>
<div id="fbar"><div class="btn" data-b="SELECT">SEL</div><div class="btn" data-b="START">START</div><div class="btn" id="fPad" title="Bedienung an den Seiten ein-/ausblenden">TASTEN</div><div class="btn" id="fSide" title="Leiste auf die andere Seite">⇆</div><div class="btn" id="fExit" title="Vollbild beenden (V)">✕</div></div>
<div id="stage">
 <div id="scr"><div id="cvw"><canvas id="cv" width="640" height="480" tabindex="0"></canvas><div id="lost"><div><div class="spin"></div><b>Verbindung verloren</b><br><span id="lostT">verbinde neu …</span><br><button id="lostR">Neu laden</button></div></div></div>
  <div class="mid" id="midL"><div class="row"><div class="btn" data-b="SELECT">SELECT</div><div class="btn" data-b="START">START</div></div></div></div>
 <div id="ctl">
  <div class="side lside"><div class="shd"><div class="btn" data-b="L2">L2</div><div class="btn" data-b="L1">L1</div></div>
   <div class="dpad" id="dpad"><i class="u">▲</i><i class="l">◀</i><i class="c"></i><i class="r">▶</i><i class="d">▼</i></div>
   <div class="stk"><span></span><div class="btn" data-b="LY" data-v="1">↑</div><span></span><div class="btn" data-b="LX" data-v="-1">←</div><span style="align-self:center;text-align:center;color:var(--dim)">L</span><div class="btn" data-b="LX" data-v="1">→</div><span></span><div class="btn" data-b="LY" data-v="-1">↓</div><span></span></div>
   <div class="btn ms" data-b="SELECT">SELECT</div></div>
  <div class="side rside"><div class="shd"><div class="btn" data-b="R1">R1</div><div class="btn" data-b="R2">R2</div></div>
   <div class="face"><div class="btn x" data-b="X">X</div><div class="btn y" data-b="Y">Y</div><div class="btn a" data-b="A">A</div><div class="btn b" data-b="B">B</div></div>
   <div class="stk"><span></span><div class="btn" data-b="RY" data-v="1">↑</div><span></span><div class="btn" data-b="RX" data-v="-1">←</div><span style="align-self:center;text-align:center;color:var(--dim)">R</span><div class="btn" data-b="RX" data-v="1">→</div><span></span><div class="btn" data-b="RY" data-v="-1">↓</div><span></span></div>
   <div class="btn ms" data-b="START">START</div></div>
 </div>
</div></div>
<div id="tui">
 <div id="tTop"><button class="tb" id="tPlay">▶</button><div class="tb" id="tPatD">‹</div><div id="tPat">A01</div><div class="tb" id="tPatU">›</div>
  <div id="tBpm">120 BPM</div><span id="tMsg"></span><span id="tLock"><span style="color:#ffba40;font-size:12px" id="tLockT">LOCK</span><button class="tb" id="tLockC">LOCKS WEG</button><button class="tb" id="tLockE">FERTIG</button></span><span id="tModes"><button class="tb tm" id="tM0">STEPS</button><button class="tb tm" id="tM1">SOUND</button><button class="tb tm" id="tM2">NOTEN</button><button class="tb tm" id="tM3">MIXER</button><button class="tb tm" id="tM4">SONG</button></span><button class="tb" id="tView" title="zum gespiegelten Geraetebild">BILD</button></div>
 <div id="tMain"><div id="tTracks"></div>
  <div id="tGrid"></div>
  <div id="tSnd"><div id="tGrps"></div><div id="tPar"></div></div>
  <div id="tKeys"><div id="tOct"><button class="tb" id="tOctD">− OKT</button><span id="tOctT" style="color:var(--cy);min-width:52px;text-align:center"></span><button class="tb" id="tOctU">+ OKT</button><span id="tNoteT" style="color:var(--dim);font-size:12px"></span></div><div id="tKb"></div></div></div>
 <div id="tMix"></div>
 <div id="tSong"><div id="tSgBar"><button class="tb" id="tSgOn">SONG AN</button><button class="tb" id="tSgAdd">+ ZEILE</button><span id="tSgI" style="color:var(--dim);font-size:12px"></span></div><div id="tSgL"></div></div>
 <div id="tWait">warte auf die Groovebox …</div>
 <div id="tPick"><div id="tPickH"><span id="tPickT"></span>
  <span style="flex:1"></span><button class="tb" id="tPickX">ABBRECHEN</button></div><div id="tPickL"></div></div>
 <div id="tBot"><div id="tPages" style="display:flex;gap:6px;flex-wrap:wrap"></div><span style="flex:1"></span><button class="tb" id="tPrj">PROJEKT</button></div>
 <div id="tSheet"><div id="tShH"><span id="tShT">PROJEKT</span><span style="flex:1"></span><button class="tb" id="tShX">ZU</button></div><div id="tShB"></div></div>
</div>
<div id="help"><div><b style="color:#fff">Tastatur</b><br>
<kbd>←↑→↓</kbd> Steuerkreuz &nbsp; <kbd>Z</kbd>/<kbd>Leertaste</kbd> A &nbsp; <kbd>X</kbd>/<kbd>⌫</kbd> B &nbsp; <kbd>S</kbd> X &nbsp; <kbd>A</kbd> Y<br>
<kbd>Q</kbd> L1 &nbsp; <kbd>W</kbd> R1 &nbsp; <kbd>1</kbd> L2 &nbsp; <kbd>2</kbd> R2 &nbsp; <kbd>Shift</kbd>/<kbd>Tab</kbd> SELECT &nbsp; <kbd>Enter</kbd> START (halten = FILL)<br>
<kbd>T F G H</kbd> linker Stick &nbsp; <kbd>I J K L</kbd> rechter Stick &nbsp; <kbd>V</kbd> Bild-Vollbild (nur das Bild, Leiste oben rechts: SEL, START, ⇆ Seite, ✕ beenden) &nbsp; <kbd>F11</kbd> Browser-Vollbild<br>
<b style="color:#fff">Zoom</b>: zwei Finger gleichzeitig aufs Bild = vergrößern/verschieben, Knöpfe − 100% + oben, am PC Strg+Mausrad (Mausrad verschiebt), Tasten <kbd>+</kbd> <kbd>−</kbd> <kbd>0</kbd>.<br><b style="color:#fff">Touch aufs Bild</b>: Spur, Seite, Reiter, Kachel oder Step antippen. Auf einer Kachel ziehen (hoch/rechts = mehr) ändert den Wert. Step antippen = Trig an/aus, Step halten + mit zweitem Finger Kachel ziehen = P-Lock. Kachel nochmal antippen = Auswahlliste (Sample, LFO-Ziel, Engine). „Tasten“ blendet das Gamepad aus.<br><b style="color:#fff">Touch-Tasten</b>: mehrere Finger gleichzeitig, Wischen über das Steuerkreuz geht auch.<br>
<b style="color:#fff">Gamepad</b>: am PC/Handy angeschlossenes Gamepad wird automatisch erkannt.<br>
<span style="color:var(--dim)">Tippen zum Schließen</span></div></div>
<script>
'use strict';
const $=id=>document.getElementById(id),cv=$('cv'),cx=cv.getContext('2d',{alpha:false});
let W=640,H=480,img=cx.createImageData(W,H),px=new Uint32Array(img.data.buffer),seq=0,dirty=false,frames=0,bytes=0,lat=0,mode='',last=performance.now();
const lut=new Uint32Array(65536);
for(let v=0;v<65536;v++){const r=(v>>11)&31,g=(v>>5)&63,b=v&31;lut[v]=0xff000000|(((b<<3)|(b>>2))<<16)|(((g<<2)|(g>>4))<<8)|((r<<3)|(r>>2));}
function decode(buf){
 const dv=new DataView(buf);seq=dv.getUint32(0,true);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);
 if(w!==W||h!==H){W=w;H=h;cv.width=w;cv.height=h;img=cx.createImageData(w,h);px=new Uint32Array(img.data.buffer);}
 const t=new Uint16Array(buf,12,(buf.byteLength-12)>>1),n=w*h;let i=0,k=0;
 while(k<t.length&&i<n){const v=t[k++],c=v&0x3fff,m=v>>14;
  if(m===0)i+=c;else if(m===1){px.fill(lut[t[k++]],i,i+c);i+=c;}else for(let j=0;j<c;j++)px[i++]=lut[t[k++]];}
 dirty=true;frames++;bytes+=buf.byteLength;
}
function draw(){if(dirty){cx.putImageData(img,0,0);dirty=false;}pollPad();requestAnimationFrame(draw);}
requestAnimationFrame(draw);
// Bild immer ganz sichtbar und so gross wie moeglich (4:3), SELECT/START darunter
function fit(){const sc=$('scr'),r=sc.getBoundingClientRect(),mh=$('midL').offsetHeight+10;let w=r.width,h=r.height-mh;
 if(w<=0||h<=0)return;if(w/h>4/3)w=h*4/3;else h=w*3/4;w=Math.floor(w);h=Math.floor(h);cv.style.width=w+'px';cv.style.height=h+'px';$('cvw').style.width=w+'px';$('cvw').style.height=h+'px';fw=w;fh=h;applyZoom();placeBar(w);}
// Die Leiste neben das Bild legen (freier Balken), sonst auf den Griff einklappen
function placeBar(w){const fb=$('fbar');if(!fb)return;
 if(!document.body.classList.contains('full')||!document.body.classList.contains('nopad')){
  fb.classList.remove('mini');fb.style.top='';fb.style.transform='';return;}
 const gap=Math.floor((innerWidth-w)/2);
 if(gap>=96){fb.classList.remove('mini');fb.style.top='50%';fb.style.transform='translateY(-50%)';}
 else{fb.style.top='';fb.style.transform='';if(!fb.dataset.open)fb.classList.add('mini');}}
$('fbar').addEventListener('pointerdown',e=>{const fb=$('fbar');if(!fb.classList.contains('mini'))return;
 e.stopPropagation();e.preventDefault();fb.classList.remove('mini');fb.dataset.open='1';
 clearTimeout(fb._t);fb._t=setTimeout(()=>{delete fb.dataset.open;placeBar(fw);},5000);},true);
addEventListener('resize',fit);if(window.ResizeObserver)new ResizeObserver(fit).observe($('scr'));setTimeout(fit,0);
// ---- Touch-Oberflaeche: eigenes UI aus dem Modell der Groovebox (keine 4:3-Bindung, grosse Flaechen)
let tuiOn=false,tm=null,tPads=[],tTrks=[],tLastKey='';
function tuiSet(on){tuiOn=on;document.body.classList.toggle('tui',on);try{localStorage.tui=on?1:'';}catch(e){}
 if(on){document.body.classList.toggle('twait',!tm);tSend('M');tuiPoll();
  clearTimeout(tuiSet._t);tuiSet._t=setTimeout(()=>{if(tuiOn&&!tm)
   $('tWait').innerHTML='Die Groovebox meldet kein Modell.<br>Beim Gerät: es braucht die neue Version (nightly).<br>In der App: Seite neu laden.<br><br>Mit <b>BILD</b> geht es zur gewohnten Ansicht.';},3000);}
 else setTimeout(fit,30);}
function tuiPoll(){if(!tuiOn)return;
 if(window.GB_LOCAL&&GB_LOCAL.model){const j=GB_LOCAL.model();if(j)tuiModel(j);}else tSend('M');
 setTimeout(tuiPoll,window.GB_LOCAL?90:1500);}
function tAct(k,a,b){tSend('k '+k+' '+(a|0)+' '+(b|0));}
function tuiModel(j){let m;try{m=typeof j==='string'?JSON.parse(j):j;}catch(e){return;}tm=m;window.tm=m;/* Tests lesen den Zustand hier */
 document.body.classList.remove('twait');tuiRender(m);}
function tuiRender(m){
 const play=$('tPlay');play.textContent=m.play?'■':'▶';play.classList.toggle('on',!!m.play);
 $('tPat').textContent=m.pat;
 if(m.prj)$('tPrj').textContent=(m.prj.d?'● ':'')+'PROJEKT';$('tBpm').textContent=m.bpm+' BPM'+(m.song?' · SONG':'');
 $('tMsg').textContent=m.msg||'';
 document.body.classList.toggle('hold',m.hold>=0);
 if(document.body.classList.contains('keys')){tKbMark(m.note);
  $('tNoteT').textContent=(m.hold>=0?'Step '+(m.hold+1)+': ':'')+tNoteName(m.note)+(m.drum?' · Drum-Engine: Tonhöhe wirkt kaum':'');}
 if(m.hold>=0)$('tLockT').textContent='LOCK STEP '+(m.hold+1);
 if(document.body.classList.contains('mix'))tMixPaint();
 if(document.body.classList.contains('song'))tSongPaint();
 // Spuren: nur neu aufbauen, wenn sich die Struktur aendert (sonst flackert es und der Finger verliert das Ziel)
 const key=m.tracks.map(t=>t.n).join(',')+'|'+m.len;
 const tr=$('tTracks');
 if(key!==tLastKey){tLastKey=key;tr.innerHTML='';tTrks=[];
  m.tracks.forEach((t,i)=>{const d=document.createElement('div');d.className='ttr';
   d.innerHTML='<span class="tnr" style="color:var(--dim);font-size:11px"></span><span class="tn"></span><span class="tm">M</span>';
   d.addEventListener('pointerdown',e=>{e.preventDefault();
    if(e.target.classList.contains('tm'))tAct(3,i,0);else tAct(2,i,0);},{passive:false});
   tr.appendChild(d);tTrks.push(d);});}
 m.tracks.forEach((t,i)=>{const d=tTrks[i];if(!d)return;
  d.style.borderLeftColor=t.c;d.classList.toggle('sel',i===m.track);
  d.querySelector('.tnr').textContent=(i+1);d.querySelector('.tn').textContent=t.n;
  d.querySelector('.tm').classList.toggle('on',!!t.m);});
 // Pads: 16 je Seite
 const base=m.page*16,n=Math.min(16,m.len-base),g=$('tGrid');
 if(tPads.length!==16){g.innerHTML='';tPads=[];
  for(let i=0;i<16;i++){const d=document.createElement('div');d.className='tp';
   // kurz = Trig an/aus, lange halten = P-Lock fuer diesen Step (wie A halten am Geraet)
   // Ein Pad muss sofort reagieren, darum wird beim Druecken getoggelt. Wird daraus ein langes
   // Halten, nimmt der zweite Toggle das zurueck und der Step haengt im P-Lock (wie A halten am Geraet).
   let ht=null;
   d.addEventListener('pointerdown',e=>{e.preventDefault();const s=(tm?tm.page:0)*16+i;
    if(!tm||s>=tm.len||document.body.classList.contains('hold'))return;
    d.classList.toggle('on');tAct(1,s,0);
    ht=setTimeout(()=>{ht=null;tAct(1,s,0);tAct(12,s,1);
     // nur aus der Pad-Ansicht zu den Reglern springen - wer von der Klaviatur kommt, will dort bleiben
     if(!document.body.classList.contains('snd')&&!document.body.classList.contains('keys'))tSetMode(1);
     if(navigator.vibrate)navigator.vibrate(12);},350);},{passive:false});
   const up=()=>{if(ht){clearTimeout(ht);ht=null;}};
   d.addEventListener('pointerup',up);d.addEventListener('pointercancel',up);
   g.appendChild(d);tPads.push(d);}}
 for(let i=0;i<16;i++){const d=tPads[i],s=base+i,st=s<m.len?m.steps[s]:-1;
  d.style.display=s<m.len?'':'none';
  d.className='tp'+(st===1?' on':'')+(st===2?' tl':'')+(s===m.now?' now':'')+(i%4===0?' beat':'')+(s===m.hold?' hold':'');
  d.style.background=st===1?m.tracks[m.track].c:'';
  d.textContent=s+1;}
 // Klang: Gruppenleiste und Regler
 if(m.grps){
  const gb=$('tGrps');
  if(gb.childElementCount!==m.grps.length||gb.dataset.v!=m.view){gb.dataset.v=m.view;gb.innerHTML='';
   m.grps.forEach((g,i)=>{const d=document.createElement('div');d.className='tg';d.textContent=g;
    d.addEventListener('pointerdown',e=>{e.preventDefault();tAct(10,i,0);},{passive:false});gb.appendChild(d);});}
  [...gb.children].forEach((d,i)=>{const on=i===m.grp;d.classList.toggle('on',on);
   if(on&&!gb.dataset.scr){gb.dataset.scr=1;d.scrollIntoView({block:'nearest',inline:'center'});setTimeout(()=>delete gb.dataset.scr,400);}});
  const pr=$('tPar'),ps=m.params||[];
  pr.classList.toggle('many',ps.length*57>pr.clientHeight);
  if(pr.childElementCount!==ps.length){pr.innerHTML='';
   ps.forEach((p,i)=>{const d=document.createElement('div');d.className='tr';
    d.innerHTML='<div class="bar"></div><span class="nm"></span><span class="vl"></span>';
    tParDrag(d,i);pr.appendChild(d);});}
  ps.forEach((p,i)=>{const d=pr.children[i];if(!d||d.dataset.drag)return;
   d.querySelector('.nm').textContent=p.n;d.querySelector('.vl').textContent=p.t;
   d.querySelector('.bar').style.width=Math.round(100*p.v/(p.mx||1))+'%';
   const pick=!!(p.opts||p.slots||p.list);
   d.classList.toggle('lk',!!p.lk);d.classList.toggle('sel',pick);
   if(pick)d.querySelector('.bar').style.width='0';});
 }
 // Seiten
 const pb=$('tPages');
 $('tBot').classList.toggle('hide',m.pages<2);
 if(pb.childElementCount!==m.pages){pb.innerHTML='';
  for(let p=0;p<m.pages;p++){const b=document.createElement('div');b.className='tb';b.textContent='P'+(p+1);
   b.addEventListener('pointerdown',e=>{e.preventDefault();tAct(6,p,0);},{passive:false});pb.appendChild(b);}}
 [...pb.children].forEach((b,p)=>b.style.borderColor=p===m.page?'var(--text)':'var(--line)');
}
// Regler ziehen: waagerecht = Wert. Die volle Breite deckt den ganzen Bereich ab; waehrend des Ziehens
// zeigt die Zeile den eigenen Wert (sonst wuerde das nachziehende Modell darunter springen).
// Auswahl: opts = Werte 0..max der Reihe nach, slots = ausdrueckliche Paare (Sample-Plaetze)
function tPickOpen(i){const p=tm.params[i];document.body.classList.add('pick');
 $('tPickT').textContent=p.n;const l=$('tPickL');l.innerHTML='';
 const shared=p.list&&tm.lists?tm.lists[p.list]:null;   // lange Listen stehen einmal je Modell
 const list=shared?shared:(p.slots?p.slots:(p.opts||[]).map((o,v)=>({v,n:o})));
 l.classList.toggle('wide',!!(p.slots||shared));   // Sample- und Zielnamen brauchen Platz
 list.forEach(o=>{const b=document.createElement('div');b.className='pk'+(o.v===p.v?' on':'');b.textContent=o.n;b.dataset.v=o.v;
  b.addEventListener('pointerdown',e=>{e.preventDefault();tAct(11,i,o.v);tPickClose();},{passive:false});
  l.appendChild(b);});
 const on=l.querySelector('.pk.on');if(on)setTimeout(()=>on.scrollIntoView({block:'nearest'}),0);}
function tPickClose(){document.body.classList.remove('pick');}
$('tPickX').addEventListener('pointerdown',e=>{e.preventDefault();tPickClose();},{passive:false});
// daneben tippen schliesst auch - ohne das blieb der Picker offen und deckte die ganze Kopfzeile zu
$('tPick').addEventListener('pointerdown',e=>{if(e.target===$('tPick')||e.target===$('tPickL')){e.preventDefault();tPickClose();}},{passive:false});
addEventListener('keydown',e=>{if(e.key!=='Escape')return;if(document.body.classList.contains('pick'))tPickClose();else if(document.body.classList.contains('sheet'))tSheetClose();});
function tParDrag(d,i){
 let id=null,x0=0,v0=0,mx=127,cur=0;
 d.addEventListener('pointerdown',e=>{if(!tm||!tm.params||!tm.params[i])return;e.preventDefault();
  if(tm.params[i].opts||tm.params[i].slots){tPickOpen(i);return;}   // Auswahlregler: Liste statt Schieber
  id=e.pointerId;x0=e.clientX;v0=cur=tm.params[i].v;mx=tm.params[i].mx||1;d.dataset.drag='1';d.classList.add('act');
  try{d.setPointerCapture(id);}catch(err){}},{passive:false});
 d.addEventListener('pointermove',e=>{if(id===null||e.pointerId!==id)return;e.preventDefault();
  const w=d.getBoundingClientRect().width||1;
  let v=Math.round(v0+(e.clientX-x0)/w*mx);v=Math.max(0,Math.min(mx,v));
  if(v!==cur){cur=v;tAct(11,i,v);
   d.querySelector('.bar').style.width=Math.round(100*v/mx)+'%';d.querySelector('.vl').textContent=v;}},{passive:false});
 const end=e=>{if(id===null||(e&&e.pointerId!==id))return;id=null;delete d.dataset.drag;d.classList.remove('act');};
 d.addEventListener('pointerup',end);d.addEventListener('pointercancel',end);
}
$('tLockE').addEventListener('pointerdown',e=>{e.preventDefault();tAct(12,0,0);},{passive:false});
$('tLockC').addEventListener('pointerdown',e=>{e.preventDefault();tAct(13,0,0);},{passive:false});
// Drei Ansichten, drei Knoepfe: STEPS, SOUND, NOTEN. Vorher war das EIN Knopf im Kreis, der das
// Ziel statt des Zustands zeigte - zu den Steps zurueck brauchte je nach Lage ein oder zwei Treffer
// (gemeldet "die Sequencer-Seite kann ich nicht zuverlaessig abrufen"). Jetzt: ein Tipp, immer.
const TMODES=['','snd','keys','mix','song'];
function tModeNow(){const c=document.body.classList;return c.contains('song')?4:(c.contains('mix')?3:(c.contains('keys')?2:(c.contains('snd')?1:0)));}
function tSetMode(m){const b=document.body;TMODES.forEach(c=>c&&b.classList.remove(c));
 if(TMODES[m])b.classList.add(TMODES[m]);
 for(let k=0;k<TMODES.length;k++){const e=$('tM'+k);if(e)e.classList.toggle('on',k===m);}
 if(TMODES[m]==='snd'&&tm&&tm.view===0)tAct(9,1,0);
 if(TMODES[m]==='keys')tKbBuild();
 if(TMODES[m]==='mix')tMixBuild();
 if(TMODES[m]==='song')tSongBuild();
 try{localStorage.tmode=m;}catch(e){}}
for(let k=0;k<TMODES.length;k++)$('tM'+k).addEventListener('pointerdown',e=>{e.preventDefault();tSetMode(k);},{passive:false});
try{const m=+(localStorage.tmode||0);setTimeout(()=>tSetMode(m||0),300);}catch(e){setTimeout(()=>tSetMode(0),300);}

// ---- Song: die Clip-Matrix. Eine Zeile je Song-Zeile, darin die acht Spuren;
// eine Zelle antippen waehlt ihr Pattern (nur belegte), die Zeilennummer spielt ab hier.
let tSgKey='';
function tSgPatName(v){const p=tm.sng.pats.find(x=>x.v===v);return p?p.n:'---';}
function tSongBuild(){const L=$('tSgL');if(!tm||!tm.sng)return;const S=tm.sng;
 L.innerHTML='';
 S.rows.forEach((r,i)=>{const d=document.createElement('div');d.className='sgr';d.dataset.i=i;
  const nr=document.createElement('div');nr.className='nr';nr.textContent=(i+1);
  nr.addEventListener('pointerdown',e=>{e.preventDefault();tAct(17,1,i);},{passive:false});d.appendChild(nr);
  const cs=document.createElement('div');cs.className='cs';
  for(let t=0;t<8;t++){const c=document.createElement('div');c.className='cl';c.dataset.t=t;
   c.addEventListener('pointerdown',e=>{e.preventDefault();tSgPick(i,t);},{passive:false});cs.appendChild(c);}
  d.appendChild(cs);
  const rp=document.createElement('div');rp.className='rp';
  const m=document.createElement('div');m.textContent='−';m.addEventListener('pointerdown',e=>{e.preventDefault();tAct(17,5,i);},{passive:false});
  const v=document.createElement('div');v.className='v';
  const p=document.createElement('div');p.textContent='+';p.addEventListener('pointerdown',e=>{e.preventDefault();tAct(17,4,i);},{passive:false});
  rp.appendChild(m);rp.appendChild(v);rp.appendChild(p);d.appendChild(rp);
  const x=document.createElement('div');x.className='del';x.textContent='✕';
  x.addEventListener('pointerdown',e=>{e.preventDefault();tAct(17,3,i);},{passive:false});d.appendChild(x);
  L.appendChild(d);});
 if(!S.rows.length){const e=document.createElement('div');e.className='lbl';e.style.color='var(--dim)';
  e.textContent='Noch keine Zeilen – "+ ZEILE" legt die erste an.';L.appendChild(e);}
 tSongPaint();}
function tSongPaint(){if(!tm||!tm.sng)return;const S=tm.sng,L=$('tSgL');
 const key=S.len+'|'+S.pats.map(p=>p.v).join(',');
 if(key!==tSgKey){tSgKey=key;tSongBuild();return;}
 $('tSgOn').textContent=S.on?'SONG AUS':'SONG AN';$('tSgOn').classList.toggle('on',!!S.on);
 $('tSgI').textContent=S.len?(S.len+' Zeilen'+(S.pos>=0?'  ·  spielt '+(S.pos+1):'')):'';
 [...L.children].forEach((d,i)=>{const r=S.rows[i];if(!r)return;
  d.classList.toggle('now',i===S.pos);
  const cl=d.querySelectorAll('.cl');
  for(let t=0;t<cl.length;t++){const v=r.p[t],nm=tSgPatName(v);
   cl[t].textContent=nm;cl[t].classList.toggle('off',nm==='---');}
  d.querySelector('.rp .v').textContent='x'+r.r;});}
// Zelle: Pattern waehlen (nur belegte, plus "schweigt")
function tSgPick(row,trk){const S=tm.sng;document.body.classList.add('pick');
 $('tPickT').textContent='ZEILE '+(row+1)+' · SPUR '+(trk+1);
 const l=$('tPickL');l.innerHTML='';l.classList.remove('wide');
 S.pats.forEach(o=>{const b=document.createElement('div');b.className='pk'+(o.v===S.rows[row].p[trk]?' on':'');
  b.textContent=o.n;b.dataset.v=o.v;
  b.addEventListener('pointerdown',e=>{e.preventDefault();tAct(18,row*16+trk,o.v);tPickClose();},{passive:false});
  l.appendChild(b);});}
$('tSgOn').addEventListener('pointerdown',e=>{e.preventDefault();tAct(17,0,0);},{passive:false});
$('tSgAdd').addEventListener('pointerdown',e=>{e.preventDefault();tAct(17,2,tm&&tm.sng?tm.sng.len-1:0);},{passive:false});

// ---- Projekt-Blatt: speichern, laden, neu. Namen tippt man am Geraet oder im Browser;
// hier gibt es "neu" mit freiem Namen - der Weg ueber eine Bildschirmtastatur waere auf dem Handy laenger.
function tSheetOpen(){document.body.classList.add('sheet');tSheetFill();}
function tSheetClose(){document.body.classList.remove('sheet');}
function tSheetFill(){const b=$('tShB');if(!tm||!tm.prj)return;const p=tm.prj;
 b.innerHTML='';
 const info=document.createElement('div');info.className='lbl';
 info.textContent='Offen: '+p.n+(p.d?'  ·  ungespeicherte Änderungen':'  ·  gespeichert');b.appendChild(info);
 const row=document.createElement('div');row.className='row';
 const sv=document.createElement('div');sv.className='big'+(p.d?' go':'');sv.textContent='SPEICHERN';
 sv.addEventListener('pointerdown',e=>{e.preventDefault();tAct(16,0,0);setTimeout(tSheetFill,600);},{passive:false});
 const nw=document.createElement('div');nw.className='big';nw.textContent='NEUES PROJEKT';
 nw.addEventListener('pointerdown',e=>{e.preventDefault();tAct(16,2,0);setTimeout(()=>{tSheetFill();tSheetClose();},700);},{passive:false});
 row.appendChild(sv);row.appendChild(nw);b.appendChild(row);
 const lb=document.createElement('div');lb.className='lbl';lb.textContent='Laden ('+(p.l?p.l.length:0)+')';b.appendChild(lb);
 const pl=document.createElement('div');pl.className='pl';
 (p.l||[]).forEach((nm,i)=>{const d=document.createElement('div');d.textContent=nm;if(nm===p.n)d.className='on';
  d.dataset.i=i;
  d.addEventListener('pointerdown',e=>{e.preventDefault();tAct(16,1,i);setTimeout(()=>{tSheetFill();tSheetClose();},900);},{passive:false});
  pl.appendChild(d);});
 b.appendChild(pl);}
$('tPrj').addEventListener('pointerdown',e=>{e.preventDefault();tSheetOpen();},{passive:false});
$('tShX').addEventListener('pointerdown',e=>{e.preventDefault();tSheetClose();},{passive:false});
$('tSheet').addEventListener('pointerdown',e=>{if(e.target===$('tSheet')||e.target===$('tShB')){e.preventDefault();tSheetClose();}},{passive:false});

// ---- Mixer: acht Spuren nebeneinander, Zeilen wie die MIX-Seite am Geraet
const MIXN=['VOL','PAN','S.A','S.B','S.C','CMP'];
function tMixBuild(){const m=$('tMix');if(!tm||!tm.tracks)return;m.innerHTML='';
 tm.tracks.forEach((t,i)=>{const c=document.createElement('div');c.className='mxc';c.dataset.t=i;
  const h=document.createElement('div');h.className='mxh';h.textContent='T'+(i+1)+' '+t.n;
  h.addEventListener('pointerdown',e=>{e.preventDefault();tAct(2,i,0);},{passive:false});c.appendChild(h);
  const b=document.createElement('div');b.className='mxb';
  ['M','S'].forEach((lb,k)=>{const d=document.createElement('div');d.textContent=lb;if(k)d.className='so';
   d.addEventListener('pointerdown',e=>{e.preventDefault();tAct(k?7:3,i,0);},{passive:false});b.appendChild(d);});
  c.appendChild(b);
  for(let r=0;r<MIXN.length;r++){const f=document.createElement('div');f.className='mxf'+(r===5?' sw':'');
   f.innerHTML='<i></i><b>'+MIXN[r]+'</b><u></u>';tMixDrag(f,i,r);c.appendChild(f);}
  m.appendChild(c);});
 tMixPaint();}
function tMixPaint(){if(!tm||!tm.tracks)return;const m=$('tMix');
 if(m.childElementCount!==tm.tracks.length){tMixBuild();return;}
 [...m.children].forEach((c,i)=>{const t=tm.tracks[i];if(!t||!t.mix)return;
  c.classList.toggle('sel',i===tm.track);
  const bs=c.querySelector('.mxb').children;bs[0].classList.toggle('on',!!t.m);bs[1].classList.toggle('on',!!t.s);
  const fs=c.querySelectorAll('.mxf');
  for(let r=0;r<fs.length;r++){const v=t.mix[r],f=fs[r];if(f.dataset.drag)continue;
   if(r===5){f.querySelector('i').style.height='0';f.querySelector('u').textContent=v?'AN':'-';
    f.style.background=v?'var(--panel2)':'var(--off)';continue;}
   f.querySelector('i').style.height=Math.round(v/127*100)+'%';f.querySelector('u').textContent=v;}});}
// senkrecht ziehen = Wert (volle Hoehe = ganzer Bereich), CMP ist ein Schalter
function tMixDrag(f,t,r){let id=null,y0=0,v0=0,cur=0;
 f.addEventListener('pointerdown',e=>{e.preventDefault();
  if(r===5){const t2=tm.tracks[t];tAct(15,t*16+r,t2.mix[r]?0:1);return;}
  id=e.pointerId;y0=e.clientY;v0=cur=tm.tracks[t].mix[r];f.setPointerCapture(id);f.dataset.drag='1';},{passive:false});
 f.addEventListener('pointermove',e=>{if(id===null||e.pointerId!==id)return;e.preventDefault();
  const h=f.getBoundingClientRect().height||1;
  let v=Math.max(0,Math.min(127,Math.round(v0+(y0-e.clientY)/h*127)));
  if(v!==cur){cur=v;f.querySelector('i').style.height=Math.round(v/127*100)+'%';f.querySelector('u').textContent=v;tAct(15,t*16+r,v);}},{passive:false});
 const end=e=>{if(id===null||(e&&e.pointerId!==id))return;id=null;delete f.dataset.drag;};
 f.addEventListener('pointerup',end);f.addEventListener('pointercancel',end);}

// ---- Klaviatur: zwei Oktaven, weisse Tasten als Reihe, schwarze darueber
let tOct=4;
const WK=[0,2,4,5,7,9,11],BK=[1,3,-1,6,8,10,-1],NN=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
function tNoteName(n){return NN[((n%12)+12)%12]+(Math.floor(n/12)-1);}
function tKbBuild(){const kb=$('tKb');if(kb.dataset.oct==tOct&&kb.childElementCount)return;kb.dataset.oct=tOct;
 kb.innerHTML='';$('tOctT').textContent='C'+(tOct-1);
 const wn=[];
 for(let o=0;o<2;o++)for(const k of WK)wn.push(tOct*12+o*12+k);
 wn.forEach(n=>{const d=document.createElement('div');d.className='wk';d.dataset.n=n;d.textContent=tNoteName(n);
  d.addEventListener('pointerdown',e=>{e.preventDefault();tAct(14,n,0);d.classList.add('on');
   setTimeout(()=>d.classList.remove('on'),180);},{passive:false});kb.appendChild(d);});
 // schwarze Tasten zwischen die weissen legen (Anteil der Gesamtbreite)
 const per=100/wn.length;
 let wi=0;
 for(let o=0;o<2;o++)for(let i=0;i<7;i++,wi++){
  const semi=BK[i];if(semi<0)continue;
  const n=tOct*12+o*12+semi;
  const d=document.createElement('div');d.className='bk';d.dataset.n=n;d.textContent=tNoteName(n);
  d.style.left='calc('+(per*(wi+1))+'% - '+(per*0.3)+'%)';d.style.width=(per*0.6)+'%';
  d.addEventListener('pointerdown',e=>{e.preventDefault();e.stopPropagation();tAct(14,n,0);d.classList.add('on');
   setTimeout(()=>d.classList.remove('on'),180);},{passive:false});kb.appendChild(d);}
}
function tKbMark(note){const kb=$('tKb');[...kb.children].forEach(d=>d.classList.toggle('cur',+d.dataset.n===note));}
$('tOctD').addEventListener('pointerdown',e=>{e.preventDefault();tOct=Math.max(0,tOct-1);tKbBuild();},{passive:false});
$('tOctU').addEventListener('pointerdown',e=>{e.preventDefault();tOct=Math.min(8,tOct+1);tKbBuild();},{passive:false});
$('tPlay').addEventListener('pointerdown',e=>{e.preventDefault();tAct(4,0,0);},{passive:false});
$('tPatD').addEventListener('pointerdown',e=>{e.preventDefault();tAct(5,-1,1);},{passive:false});
$('tPatU').addEventListener('pointerdown',e=>{e.preventDefault();tAct(5,1,1);},{passive:false});
$('tView').addEventListener('pointerdown',e=>{e.preventDefault();tuiSet(false);},{passive:false});
try{if(localStorage.tui)setTimeout(()=>tuiSet(true),400);}catch(e){}
// ---- Verbindung: WebSocket (Bildstrom + Tasten), sonst Abfrage
let ws=null,wsFails=0,everWs=false,pingT=0,lastRx=performance.now(),lostAt=0,tries=0;
// Verbindungsverlust sichtbar machen: Bild abdunkeln + Hinweis statt eingefrorenem Bild
function setLost(on){const el=$('lost');if(on){if(!lostAt){lostAt=performance.now();tries=0;}el.classList.add('on');}
 else{lostAt=0;el.classList.remove('on');}}
function lostTick(){if(!lostAt)return;const s=Math.round((performance.now()-lostAt)/1000);
 $('lostT').textContent='seit '+s+' s · Versuch '+tries+(navigator.onLine===false?' · Handy offline':' · WLAN/USB prüfen');}
function setStat(ok,txt){$('dot').style.background=ok?'var(--gr)':'#c33';if(txt)$('stat').textContent=txt;setLost(!ok);}
$('lostR').onclick=e=>{e.stopPropagation();location.reload();};
// Watchdog: der R36S beantwortet den Ping jede Sekunde - kommt 5 s nichts (grosses Bild ueber langsames WLAN braucht etwas), ist die Leitung tot
// (WLAN weg, Gerät aus, Handy aus dem Ruhezustand): Socket verwerfen und neu verbinden
function wsKill(){if(!ws)return;const w=ws;ws=null;w.onclose=w.onmessage=w.onopen=null;try{w.close();}catch(e){}
 if(mode==='ws')mode='';setStat(false,'getrennt – verbinde neu …');releaseAll();setTimeout(connect,300);
 if(aws){const a=aws;aws=null;a.onclose=a.onmessage=null;try{a.close();}catch(e){}if(aOn)setTimeout(connectAudio,800);}}   // Ton-Socket haengt meist mit
setInterval(()=>{lostTick();if(ws&&performance.now()-lastRx>5000)wsKill();},500);
function connect(){
 if(window.GB_LOCAL){mode='local';setStat(true);GB_LOCAL.start();return;}   // Handy-App: Groovebox laeuft in dieser Seite (web/local.js)
 if(wsFails>=3&&!everWs){mode='poll';pollLoop();return;}
 tries++;
 try{ws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws');}catch(e){wsFails++;setTimeout(connect,500);return;}
 ws.binaryType='arraybuffer';
 const me=ws;lastRx=performance.now();
 const to=setTimeout(()=>{if(ws===me&&me.readyState!==1)wsKill();},4000);   // Aufbau haengt (kein Netz)
 ws.onopen=()=>{clearTimeout(to);mode='ws';everWs=true;wsFails=0;lastRx=performance.now();setStat(true);ws.send('f');};
 ws.onmessage=e=>{lastRx=performance.now();if(typeof e.data==='string'){if(e.data[0]==='P')lat=performance.now()-(+e.data.slice(2));else if(e.data[0]==='m'&&e.data[1]===' ')tuiModel(e.data.slice(2));}else decode(e.data);};
 ws.onclose=()=>{clearTimeout(to);if(ws!==me)return;ws=null;if(mode==='ws')mode='';wsFails++;setStat(false,'getrennt – verbinde neu …');releaseAll();setTimeout(connect,600);};
}
async function pollLoop(){
 for(;;){tries++;try{const ac=window.AbortController?new AbortController():null,tm=setTimeout(()=>{if(ac)ac.abort();},4000);
   const r=await fetch('/api/screen?since='+seq,{cache:'no-store',signal:ac?ac.signal:undefined});clearTimeout(tm);
   if(r.status===200){decode(await r.arrayBuffer());}else if(r.status===204){await new Promise(z=>setTimeout(z,20));}else throw 0;
   setStat(true);}catch(e){setStat(false,'keine Verbindung …');seq=0;await new Promise(z=>setTimeout(z,800));}}
}
// ---- Ton: eigener WebSocket /wsa (16 Bit Stereo), Pakete werden luecklos hintereinander eingeplant.
// Puffer ~120 ms; laeuft er voll (Uhren von R36S und Handy weichen ab), wird minimal schneller abgespielt, bei Luecke neu gepuffert.
let actx=null,aws=null,again=null,nextT=0,aOn=false,aBuf=0,aDrop=0;const TGT=0.12;
function audioOn(){
 if(!actx){const AC=window.AudioContext||window.webkitAudioContext;if(!AC){alert('Dieser Browser kann keinen Ton abspielen');return;}
  actx=new AC({latencyHint:'interactive'});again=actx.createGain();again.connect(actx.destination);}
 actx.resume();again.gain.value=($('vol').value/100)**2;aOn=true;$('bAud').classList.add('on');$('bAud').textContent='🔊 Ton';$('vol').style.display='';
 try{localStorage.audio=1;}catch(e){}
 connectAudio();
}
function audioOff(){aOn=false;if(aws){aws.onclose=null;aws.close();aws=null;}$('bAud').classList.remove('on');$('bAud').textContent='🔈 Ton';$('vol').style.display='none';try{localStorage.audio='';}catch(e){}}
function connectAudio(){
 if(!aOn||aws)return;
 aws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/wsa');aws.binaryType='arraybuffer';
 aws.onmessage=e=>{if(typeof e.data==='string'||!actx)return;
  const dv=new DataView(e.data),rate=dv.getUint32(0,true),n=(e.data.byteLength-8)>>2;if(n<=0)return;
  const pcm=new Int16Array(e.data,8,n*2),b=actx.createBuffer(2,n,rate),L=b.getChannelData(0),R=b.getChannelData(1);
  for(let i=0;i<n;i++){L[i]=pcm[i*2]/32768;R[i]=pcm[i*2+1]/32768;}
  const now=actx.currentTime;
  if(nextT<now+0.01){nextT=now+TGT;}
  let lead=nextT-now;if(lead>TGT*3){aDrop++;return;}
  const pr=lead>TGT*1.4?1.004:(lead<TGT*0.6?0.997:1);
  const src=actx.createBufferSource();src.buffer=b;src.playbackRate.value=pr;src.connect(again);src.start(nextT);
  nextT+=n/rate/pr;aBuf=lead;};
 aws.onclose=()=>{aws=null;if(aOn)setTimeout(connectAudio,800);};
}
$('bAud').onclick=()=>{if(aOn)audioOff();else audioOn();};
$('vol').oninput=()=>{if(again)again.gain.value=($('vol').value/100)**2;try{localStorage.vol=$('vol').value;}catch(e){}};
try{if(localStorage.vol)$('vol').value=localStorage.vol;}catch(e){}
try{if(localStorage.audio){const once=ev=>{document.removeEventListener('pointerdown',once,true);if(!aOn&&ev.target.id!=='bAud')audioOn();};document.addEventListener('pointerdown',once,true);$('bAud').textContent='🔈 Ton (antippen)';}}catch(e){}
if(!window.GB_LOCAL&&'serviceWorker' in navigator&&window.isSecureContext)navigator.serviceWorker.register('/sw.js').catch(()=>{});
setInterval(()=>{const now=performance.now(),s=(now-last)/1000;
 if(ws&&ws.readyState===1)ws.send('p '+now.toFixed(0));
 if(aws&&aws.readyState===1)aws.send('p');   // Lebenszeichen auch fuer den Ton (Geraet schliesst stille Verbindungen)
 $('stat').textContent=mode==='local'?'Offline · '+Math.round(frames/s)+' fps':(mode==='ws'?'Live':mode==='poll'?'Abfrage':'…')+' · '+Math.round(frames/s)+' fps · '+Math.round(bytes/1024/s)+' kB/s'+(mode==='ws'?' · '+Math.round(lat)+' ms':'')+(aOn&&aws?' · Ton '+Math.round(aBuf*1000)+' ms':'');
 frames=0;bytes=0;last=now;},1000);
document.addEventListener('visibilitychange',()=>{if(document.hidden){releaseAll();return;}
 lastRx=performance.now();if(ws&&ws.readyState===1){ws.send('f');ws.send('p '+lastRx.toFixed(0));}});   // zurueck aus dem Hintergrund: Watchdog prueft sofort
addEventListener('online',()=>{if(ws&&ws.readyState!==1)wsKill();});
addEventListener('offline',()=>{if(ws)wsKill();});
connect();
// ---- Tasten senden (Zaehler je Taste: Touch + Tastatur + Gamepad duerfen sich ueberlappen)
const cnt={},AX={LX:1,LY:1,RX:1,RY:1};
function vib(){if(navigator.vibrate)try{navigator.vibrate(8);}catch(e){}}
function out(msg,b,v){if(window.GB_LOCAL){GB_LOCAL.send(msg);return;}if(ws&&ws.readyState===1)ws.send(msg);else fetch('/api/input?b='+b+'&v='+v,{method:'POST',keepalive:true}).catch(()=>{});}
function press(b,v){const key=b+(AX[b]?':'+v:'');cnt[key]=(cnt[key]||0)+1;if(cnt[key]>1)return;
 if(AX[b])out('a '+b+' '+v,b,v);else out('d '+b,b,1);show(key,true);vib();}
function release(b,v){const key=b+(AX[b]?':'+v:'');if(!cnt[key])return;if(--cnt[key]>0)return;delete cnt[key];
 if(AX[b])out('a '+b+' 0',b,0);else out('u '+b,b,0);show(key,false);}
function releaseAll(){for(const k in cnt){const[b,v]=k.split(':');cnt[k]=1;release(b,v===undefined?1:+v);}touches.clear();}
const dEl={UP:'.u',DOWN:'.d',LEFT:'.l',RIGHT:'.r'};
function show(key,on){const[b,v]=key.split(':');
 if(dEl[b]){$('dpad').querySelector(dEl[b]).classList.toggle('on',on);return;}
 document.querySelectorAll('[data-b="'+b+'"]'+(v!==undefined?'[data-v="'+v+'"]':'')).forEach(e=>e.classList.toggle('on',on));}
// ---- Touch/Maus: jeder Finger haelt eine Taste (bzw. eine Steuerkreuz-Richtung), Wischen wechselt die Taste
const touches=new Map();   // pointerId -> Liste [b,v]
function targetsAt(x,y){
 const d=$('dpad').getBoundingClientRect();
 if(x>=d.left&&x<=d.right&&y>=d.top&&y<=d.bottom){
  const dx=x-(d.left+d.width/2),dy=y-(d.top+d.height/2),r=Math.hypot(dx,dy);if(r<d.width*0.12)return[];
  const a=Math.atan2(dy,dx)*180/Math.PI,res=[];   // 8 Richtungen: Diagonale = zwei Tasten
  if(a>-67.5&&a<67.5)res.push(['RIGHT',1]);if(a>112.5||a<-112.5)res.push(['LEFT',1]);
  if(a>22.5&&a<157.5)res.push(['DOWN',1]);if(a<-22.5&&a>-157.5)res.push(['UP',1]);return res;}
 const el=document.elementFromPoint(x,y),bt=el&&el.closest('[data-b]');
 return bt?[[bt.dataset.b,+(bt.dataset.v||1)]]:[];
}
function same(a,b){return a.length===b.length&&a.every((t,i)=>t[0]===b[i][0]&&t[1]===b[i][1]);}
function setTouch(id,list){const old=touches.get(id)||[];if(same(old,list))return;
 old.forEach(t=>{if(!list.some(n=>n[0]===t[0]&&n[1]===t[1]))release(t[0],t[1]);});
 list.forEach(t=>{if(!old.some(o=>o[0]===t[0]&&o[1]===t[1]))press(t[0],t[1]);});
 if(list.length)touches.set(id,list);else touches.delete(id);}
const ctlArea=[$('ctl'),$('midL'),$('fbar')];
ctlArea.forEach(area=>{
 area.addEventListener('pointerdown',e=>{const l=targetsAt(e.clientX,e.clientY);if(!l.length)return;e.preventDefault();try{area.setPointerCapture(e.pointerId);}catch(_){}setTouch(e.pointerId,l);});
 area.addEventListener('pointermove',e=>{if(!touches.has(e.pointerId))return;e.preventDefault();setTouch(e.pointerId,targetsAt(e.clientX,e.clientY));});
 const end=e=>{setTouch(e.pointerId,[]);};
 area.addEventListener('pointerup',end);area.addEventListener('pointercancel',end);area.addEventListener('lostpointercapture',end);
});
document.addEventListener('contextmenu',e=>e.preventDefault());
document.addEventListener('touchmove',e=>{if(e.touches.length>1||e.target.closest('#ctl,#midL,#fbar'))e.preventDefault();},{passive:false});
// ---- Zoom: Bild bis 4x vergroessern, Ausschnitt verschieben (Transform auf dem Canvas; tPos rechnet ueber getBoundingClientRect weiter richtig)
let zm=1,zx=0,zy=0,fw=640,fh=480;
function applyZoom(){zm=Math.max(1,Math.min(4,zm));zx=Math.max(0,Math.min(fw*(zm-1),zx));zy=Math.max(0,Math.min(fh*(zm-1),zy));
 cv.style.transform=zm>1?'translate('+(-zx)+'px,'+(-zy)+'px) scale('+zm+')':'';$('zl').textContent=Math.round(zm*100)+'%';$('zl').classList.toggle('on',zm>1);}
function zoomAt(nz,cx,cy){   // cx/cy: Punkt im sichtbaren Ausschnitt (CSS-Pixel), der an seiner Stelle bleibt
 nz=Math.max(1,Math.min(4,nz));const bx=(cx+zx)/zm,by=(cy+zy)/zm;zm=nz;zx=bx*zm-cx;zy=by*zm-cy;applyZoom();}
function zoomStep(f){zoomAt(zm*f,fw/2,fh/2);}
$('zi').onclick=()=>zoomStep(1.25);$('zo').onclick=()=>zoomStep(0.8);$('zl').onclick=()=>{zm=1;zx=zy=0;applyZoom();};
$('cvw').addEventListener('wheel',e=>{const r=$('cvw').getBoundingClientRect();
 if(e.ctrlKey||e.metaKey){e.preventDefault();zoomAt(zm*Math.exp(-e.deltaY*0.0025),e.clientX-r.left,e.clientY-r.top);}
 else if(zm>1){e.preventDefault();zx+=e.deltaX;zy+=e.deltaY;applyZoom();}},{passive:false});
addEventListener('keydown',e=>{if(e.ctrlKey||e.metaKey||e.altKey)return;
 if(e.key==='+'||e.code==='NumpadAdd'||e.code==='Equal'){e.preventDefault();zoomStep(1.25);}
 else if(e.key==='-'||e.code==='NumpadSubtract'||e.code==='Minus'){e.preventDefault();zoomStep(0.8);}
 else if(e.code==='Digit0'||e.code==='Numpad0'){e.preventDefault();zm=1;zx=zy=0;applyZoom();}
 else if(e.code==='KeyV'){e.preventDefault();setFull(!document.body.classList.contains('full'));}});
// Zwei Finger, die fast gleichzeitig (< 150 ms) aufs Bild kommen = Zoomen/Verschieben; spaeter dazukommender Finger = normale
// Bedienung (Step halten + Kachel ziehen). Dafuer wird der erste Touch-Finger bis zu 150 ms zurueckgehalten.
const pend=new Map();let gest=null;
function cvwXY(e){const r=$('cvw').getBoundingClientRect();return[e.clientX-r.left,e.clientY-r.top];}
function gestStart(){const p=[...pend.values()];pend.forEach(q=>clearTimeout(q.tm));pend.clear();
 const[a,b]=p;gest={ids:[a.pid,b.pid],pos:{[a.pid]:a.xy,[b.pid]:b.xy}};gest.d0=Math.hypot(a.xy[0]-b.xy[0],a.xy[1]-b.xy[1])||1;gest.z0=zm;
 gest.m0=[(a.xy[0]+b.xy[0])/2,(a.xy[1]+b.xy[1])/2];gest.zx0=zx;gest.zy0=zy;}
function gestMove(){const[a,b]=gest.ids.map(i=>gest.pos[i]);const d=Math.hypot(a[0]-b[0],a[1]-b[1]),m=[(a[0]+b[0])/2,(a[1]+b[1])/2];
 const nz=Math.max(1,Math.min(4,gest.z0*d/gest.d0));const bx=(gest.m0[0]+gest.zx0)/gest.z0,by=(gest.m0[1]+gest.zy0)/gest.z0;   // Bildpunkt unter der Anfangsmitte
 zm=nz;zx=bx*zm-m[0];zy=by*zm-m[1];applyZoom();}
// ---- Touch direkt auf dem Bild: Kacheln, Steps, Spuren, Reiter ... (die Groovebox wertet die Stelle aus)
const tIds=new Map(),tPend=new Map(),rips=new Map();
function tId(pid){if(tIds.has(pid))return tIds.get(pid);for(let i=0;i<10;i++){if(![...tIds.values()].includes(i)){tIds.set(pid,i);return i;}}return -1;}
function tPos(e){const r=cv.getBoundingClientRect();return[Math.max(0,Math.min(W-1,Math.round((e.clientX-r.left)/r.width*W))),Math.max(0,Math.min(H-1,Math.round((e.clientY-r.top)/r.height*H)))];}
function tSend(m){if(window.GB_LOCAL){GB_LOCAL.send(m);return;}if(ws&&ws.readyState===1)ws.send(m);}
function ripple(e,hold){const r=$('cvw').getBoundingClientRect(),d=document.createElement('div');d.className='rip'+(hold?' hold':'');d.style.left=(e.clientX-r.left)+'px';d.style.top=(e.clientY-r.top)+'px';$('cvw').appendChild(d);if(!hold)setTimeout(()=>d.remove(),460);return d;}
function tDown(e){const id=tId(e.pointerId);if(id<0)return;
 const[x,y]=tPos(e);tSend('t '+id+' '+x+' '+y);ripple(e,false);rips.set(e.pointerId,ripple(e,true));vib();}
function commit(pid){const q=pend.get(pid);if(!q)return;clearTimeout(q.tm);pend.delete(pid);tDown(q.ev);}
cv.addEventListener('pointerdown',e=>{e.preventDefault();try{cv.setPointerCapture(e.pointerId);}catch(_){}
 if(gest)return;
 if(e.pointerType==='touch'&&!tIds.size){   // erster Finger: kurz zurueckhalten (Geste?)
  if(pend.size===1){pend.set(e.pointerId,{pid:e.pointerId,xy:cvwXY(e),ev:e});gestStart();return;}
  const q={pid:e.pointerId,xy:cvwXY(e),ev:e,x0:e.clientX,y0:e.clientY};q.tm=setTimeout(()=>commit(e.pointerId),150);pend.set(e.pointerId,q);return;}
 tDown(e);});
cv.addEventListener('pointermove',e=>{
 if(gest&&gest.pos[e.pointerId]){e.preventDefault();gest.pos[e.pointerId]=cvwXY(e);gestMove();return;}
 const q=pend.get(e.pointerId);if(q){if(Math.hypot(e.clientX-q.x0,e.clientY-q.y0)>10)commit(e.pointerId);else return;}
 if(!tIds.has(e.pointerId))return;e.preventDefault();tPend.set(e.pointerId,tPos(e));const r=rips.get(e.pointerId);
 if(r){const b=$('cvw').getBoundingClientRect();r.style.left=(e.clientX-b.left)+'px';r.style.top=(e.clientY-b.top)+'px';}});
function tEnd(e){
 if(gest&&gest.pos[e.pointerId]){delete gest.pos[e.pointerId];if(!Object.keys(gest.pos).length)gest=null;return;}
 if(pend.has(e.pointerId))commit(e.pointerId);
 if(!tIds.has(e.pointerId))return;const id=tIds.get(e.pointerId),[x,y]=tPos(e);tPend.delete(e.pointerId);tSend('e '+id+' '+x+' '+y);tIds.delete(e.pointerId);
 const r=rips.get(e.pointerId);if(r){r.remove();rips.delete(e.pointerId);}}
cv.addEventListener('pointerup',tEnd);cv.addEventListener('pointercancel',tEnd);
setInterval(()=>{for(const[pid,p]of tPend){const id=tIds.get(pid);if(id!==undefined)tSend('m '+id+' '+p[0]+' '+p[1]);}tPend.clear();},16);   // Bewegung hoechstens ~60x/s
// ---- Tastatur
const km={ArrowUp:['UP',1],ArrowDown:['DOWN',1],ArrowLeft:['LEFT',1],ArrowRight:['RIGHT',1],KeyZ:['A',1],KeyY:['A',1],Space:['A',1],KeyX:['B',1],Backspace:['B',1],Escape:['B',1],
 KeyS:['X',1],KeyA:['Y',1],KeyQ:['L1',1],KeyW:['R1',1],Digit1:['L2',1],Digit2:['R2',1],ShiftLeft:['SELECT',1],ShiftRight:['SELECT',1],Tab:['SELECT',1],
 Enter:['START',1],NumpadEnter:['START',1],KeyT:['LY',1],KeyG:['LY',-1],KeyF:['LX',-1],KeyH:['LX',1],KeyI:['RY',1],KeyK:['RY',-1],KeyJ:['RX',-1],KeyL:['RX',1]};
const kh={};
addEventListener('keydown',e=>{if(e.ctrlKey||e.metaKey||e.altKey)return;const m=km[e.code];if(!m)return;e.preventDefault();if(kh[e.code])return;kh[e.code]=1;press(m[0],m[1]);});
addEventListener('keyup',e=>{const m=km[e.code];if(!m||!kh[e.code])return;e.preventDefault();delete kh[e.code];release(m[0],m[1]);});
addEventListener('blur',()=>{for(const c in kh){release(km[c][0],km[c][1]);delete kh[c];}});
// ---- Gamepad (Standard-Belegung, Tastenlage wie am R36S: rechts A, unten B, oben X, links Y)
const gpMap={1:'A',0:'B',3:'X',2:'Y',4:'L1',5:'R1',6:'L2',7:'R2',8:'SELECT',9:'START',12:'UP',13:'DOWN',14:'LEFT',15:'RIGHT'};
const gpState={};
function pollPad(){const gps=navigator.getGamepads?navigator.getGamepads():[];
 for(const gp of gps){if(!gp)continue;
  for(const i in gpMap){const p=gp.buttons[i]&&gp.buttons[i].pressed,k='g'+gp.index+'b'+i;if(p&&!gpState[k]){gpState[k]=1;press(gpMap[i],1);}else if(!p&&gpState[k]){delete gpState[k];release(gpMap[i],1);}}
  [['LX',0,1],['LY',1,-1],['RX',2,1],['RY',3,-1]].forEach(([n,ai,sg])=>{const a=gp.axes[ai]||0,v=a>0.5?sg:(a<-0.5?-sg:0),k='g'+gp.index+n,o=gpState[k]||0;
   if(v!==o){if(o)release(n,o);if(v)press(n,v);if(v)gpState[k]=v;else delete gpState[k];}});}}
// ---- Kopfzeile
const togglePad=()=>{document.body.classList.toggle('nopad');try{localStorage.nopad=document.body.classList.contains('nopad')?1:'';}catch(e){}setTimeout(fit,30);};
$('bPad').onclick=togglePad;
$('bTui').onclick=()=>tuiSet(true);
$('fPad').addEventListener('pointerdown',e=>{e.stopPropagation();e.preventDefault();togglePad();});
try{if(localStorage.nopad)document.body.classList.add('nopad');}catch(e){}
$('bHelp').onclick=()=>$('help').classList.add('show');$('help').onclick=()=>$('help').classList.remove('show');
$('bSt').onclick=()=>{document.body.classList.toggle('sticks');try{localStorage.sticks=document.body.classList.contains('sticks')?1:'';}catch(e){}};
try{if(localStorage.sticks)document.body.classList.add('sticks');}catch(e){}
let wl=null;async function wake(){try{if(navigator.wakeLock&&!wl){wl=await navigator.wakeLock.request('screen');wl.addEventListener('release',()=>wl=null);}}catch(e){}}
document.addEventListener('pointerdown',wake);document.addEventListener('keydown',wake);
// Bild-Vollbild: Kopfzeile und Tasten weg, Bild fuellt den Bildschirm (4:3), wo moeglich auch Browser-Vollbild
function setFull(on){document.body.classList.toggle('full',on);try{localStorage.full=on?1:'';}catch(e){}
 const d=document.documentElement,fs=document.fullscreenElement||document.webkitFullscreenElement;
 try{if(on&&!fs&&(d.requestFullscreen||d.webkitRequestFullscreen)){const r=(d.requestFullscreen||d.webkitRequestFullscreen).call(d);if(r&&r.catch)r.catch(()=>{});}
  else if(!on&&fs)(document.exitFullscreen||document.webkitExitFullscreen).call(document);}catch(e){}
 setTimeout(fit,60);setTimeout(fit,400);}
$('bFull').onclick=()=>setFull(true);
$('fExit').addEventListener('pointerdown',e=>{e.stopPropagation();e.preventDefault();setFull(false);});
$('fSide').addEventListener('pointerdown',e=>{e.stopPropagation();e.preventDefault();document.body.classList.toggle('fleft');});
try{if(localStorage.full)document.body.classList.add('full');}catch(e){}
$('bFs').onclick=()=>{const d=document.documentElement;if(document.fullscreenElement||document.webkitFullscreenElement){(document.exitFullscreen||document.webkitExitFullscreen).call(document);}
 else{(d.requestFullscreen||d.webkitRequestFullscreen).call(d);try{screen.orientation.lock('landscape').catch(()=>{});}catch(e){}}};
</script></body></html>)HTML";
