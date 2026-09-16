#pragma once
constexpr char READER_HTML[] = R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="paper-token" content="{{TOKEN}}"><title>Paper OS · Books</title><style>
*{box-sizing:border-box}body{font:17px system-ui;background:#f4f1e8;color:#19231e;margin:0}main{max-width:850px;margin:auto;padding:28px}h1{font:48px Georgia,serif}h2{font:28px Georgia,serif}section{border-top:2px solid #195d3b;margin-top:24px;padding-top:14px}button,input,select{font:inherit;padding:10px;margin:6px 4px 6px 0;max-width:100%}button{background:#195d3b;color:white;border:0;border-radius:6px;cursor:pointer}button:disabled{opacity:.5}label{display:block}a{color:#195d3b}pre{white-space:pre-wrap;max-height:220px;overflow:auto;font:18px Georgia,serif;line-height:1.6}#message{min-height:3em;white-space:pre-wrap}li{margin:16px 0;overflow-wrap:anywhere}small{color:#526158}
</style><main><nav><a href="/gallery">Pictures</a> / Books / <a href="/device">Device lab</a></nav><h1>A page at a time.</h1><p>Bring a plain-text book. Your place and bookmark are saved on the device, even when it is unplugged.</p>
<section><h2>Import a book</h2><label>TXT file <input id="file" type="file" accept=".txt,text/plain"></label><label>Text encoding <select id="encoding"><option value="utf-8">UTF-8</option><option value="windows-1252">Windows / Western text</option><option value="utf-16le">UTF-16 little-endian</option><option value="utf-16be">UTF-16 big-endian</option></select></label>
<label><input id="reflow" type="checkbox" checked> Join wrapped lines within paragraphs (turn off for poetry)</label><label>Title <input id="title" maxlength="64" placeholder="Book title"></label>
<p><small>Up to 1 MiB of prepared text, 64 books. These fonts support Latin text: accents are transliterated, smart punctuation simplified. Other scripts are rejected explicitly. Original files stay on your computer.</small></p><pre id="preview">Choose a file to preview the prepared text.</pre><button id="upload" disabled>Save book to SD</button></section>
<p id="message" role="status" aria-live="polite"></p><section><h2>Library</h2><button id="refresh">Refresh library</button><ul id="library"></ul></section>
<section><h2>Reading controls</h2><p id="state">Checking reader…</p><button data-action="previous">Previous page</button><button data-action="next">Next page</button><button data-action="resume">Continue reading</button><button data-action="menu">Device menu</button><br>
<input id="page" type="number" min="1" value="1" aria-label="Page number"><button id="jump">Go to page</button><button data-action="bookmark">Set bookmark</button><button data-action="recall">Go to bookmark</button>
<label>Reading font <select id="font"><option value="0">Book serif · 12 pt</option><option value="1">Book serif · 9 pt</option><option value="2">Book serif · 18 pt</option><option value="3">Clean sans · 12 pt</option><option value="4">Typewriter · 12 pt</option></select></label><button id="applyFont">Apply font</button><button data-action="sampler">Display font comparison sheet</button>
<p>On the device: A (upper-left) goes back, B (lower-left) goes forward, C (top) opens the menu or selects an item. Hold A to bookmark. Hold B for Wi-Fi setup; hold C for updates. Short presses act on release.</p>
<p>Allow 15–30 seconds for each physical page refresh. Buttons and requests while the reader is busy are ignored with a busy cue. Changing fonts keeps you near the same text; page numbers may change.</p></section></main><script src="/reader.js"></script></html>)HTML";

constexpr char READER_JS[] = R"JS(
'use strict';
function prepareText(input,reflow=true){
  let s=input.replace(/^\uFEFF/,'').replace(/\r\n?/g,'\n').replace(/\t/g,'    ')
    .replace(/[\u2018\u2019]/g,"'").replace(/[\u201C\u201D]/g,'"').replace(/[\u2013\u2014]/g,'--')
    .replace(/\u2026/g,'...').replace(/\u00A0/g,' ').replace(/\u00AD/g,'')
    .replace(/ß/g,'ss').replace(/Æ/g,'AE').replace(/æ/g,'ae').replace(/Œ/g,'OE').replace(/œ/g,'oe')
    .replace(/Ø/g,'O').replace(/ø/g,'o').normalize('NFKD').replace(/[\u0300-\u036f]/g,'');
  if(/[^\x20-\x7e\n]/.test(s))throw Error('This book contains unsupported characters. This first reader supports Latin text; check the encoding or use another book.');
  if(reflow)s=s.split(/\n[ \n]*\n/).map(p=>p.replace(/\n/g,' ').replace(/ +/g,' ').trim()).join('\n\n');
  s=s.trim()+'\n';
  if(!s.trim())throw Error('The book is empty.');
  if(s.length>1048576)throw Error('Prepared text is larger than 1 MiB. Split this book into volumes.');
  return s;
}
if(typeof module!=='undefined')module.exports={prepareText};
if(typeof document!=='undefined'){
  const $=id=>document.getElementById(id),token=document.querySelector('meta[name="paper-token"]').content;
  let prepared=null,revision=0;
  const say=s=>$('message').textContent=s;
  async function request(path,options={}){const r=await fetch(path,{...options,headers:{...options.headers,'X-Paper-Token':token}});const data=await r.json();if(!r.ok)throw Error(data.error||'Request failed');return data;}
  async function prepare(){
    const current=++revision;prepared=null;$('upload').disabled=true;
    try{
      const f=$('file').files[0];if(!f)return;if(f.size>2097152)throw Error('Source file exceeds 2 MiB. Split it into volumes.');
      const raw=new TextDecoder($('encoding').value,{fatal:true}).decode(await f.arrayBuffer());
      const text=prepareText(raw,$('reflow').checked);if(current!==revision)return;
      prepared=text;$('preview').textContent=text.slice(0,1600);$('upload').disabled=false;
      if(!$('title').value)$('title').value=prepareText(f.name.replace(/\.txt$/i,''),false).trim().slice(0,64);
      say(`Ready: ${text.length.toLocaleString()} bytes. Preview shows the normalized text that will be stored.`);
    }catch(e){if(current===revision)say(e.message);}
  }
  $('file').onchange=()=>{$('title').value='';prepare();};$('encoding').onchange=prepare;$('reflow').onchange=prepare;
  async function action(action,extra={}){try{await request('/api/reader/action',{method:'POST',body:new URLSearchParams({action,...extra})});say('Reader request accepted. Let the screen finish refreshing.');}catch(e){say(e.message);}}
  async function library(){try{const data=await request('/api/books');$('library').replaceChildren();for(const book of data.books.sort((a,b)=>a.title.localeCompare(b.title))){const li=document.createElement('li'),button=document.createElement('button'),link=document.createElement('a');button.textContent='Read '+book.title;button.onclick=()=>action('open',{id:book.id});link.textContent='Download TXT';link.href='/api/books/download?id='+book.id;li.append(button,' ',link);$('library').append(li);}if(!data.books.length)$('library').textContent='No books yet. Import a TXT file above.';}catch(e){say(e.message);}}
  $('upload').onclick=async()=>{if(!prepared)return;$('upload').disabled=true;try{const title=prepareText($('title').value,false).trim();if(title.length>64||title.includes('\n'))throw Error('Use a title of 1–64 characters on one line.');const body=new FormData();body.append('book',new Blob([prepared],{type:'text/plain'}),'book.txt');const data=await request('/api/books/upload?title='+encodeURIComponent(title),{method:'POST',body});await library();say('Saved '+data.title+'. Choose Read in the library.');}catch(e){say(e.message);}finally{$('upload').disabled=!prepared;}};
  $('refresh').onclick=library;
  for(const button of document.querySelectorAll('[data-action]'))button.onclick=()=>action(button.dataset.action);
  $('applyFont').onclick=()=>action('font',{value:$('font').value});$('jump').onclick=()=>action('jump',{value:$('page').value});
  async function poll(){try{const s=await request('/api/reader');$('state').textContent=(s.title?`${s.title} · page ${s.page} of ${s.pages}`:'No book open')+(s.busy?' · refreshing…':' · ready')+(s.message?' · '+s.message:'');for(const b of document.querySelectorAll('[data-action],#applyFont,#jump'))b.disabled=s.busy;$('page').max=Math.max(1,s.pages);}catch(e){$('state').textContent=e.message;}}
  library();poll();setInterval(poll,2000);
}
)JS";
