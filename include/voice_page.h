#pragma once
constexpr char VOICE_HTML[]=R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="paper-token" content="{{TOKEN}}"><title>Paper OS · Voice notes</title>
<style>body{font:17px system-ui;max-width:760px;margin:auto;padding:24px;background:#f4f1e8;color:#19231e}button,input{font:inherit;padding:12px;margin:6px}li{margin:20px 0}audio{display:block;max-width:100%}#status{white-space:pre-wrap}</style>
<a href="/gallery">Pictures</a> / <a href="/books">Books</a> / <a href="/device">Device</a><h1>Voice notes</h1>
<p>Records the PaperColor's microphone, not your phone. Five-minute limit; 16 kHz mono WAV. Keep the SD card inserted and do not power off while recording or saving. Recordings stay on your SD card; anyone on the same network who can access this device can listen to saved notes.</p>
<button id="start">Start recording</button><button id="stop">Stop &amp; save</button><p id="status" role="status">Loading…</p>
<p>On the device: choose Voice notes, then A to record. While recording, release any user button to stop. The dim blinking LED means recording is active. No screen refresh during recording.</p>
<h2>Clock</h2><button id="clock">Use phone/computer time</button><p>Filenames use UTC. Set the clock once before recording; the battery-backed clock keeps time offline.</p>
<h2>Saved notes</h2><input id="month" type="month"><button id="list">Load month</button><ul id="files"></ul><p>Shows the newest 100 notes in the selected month. Interrupted .part files are retained on SD for recovery, not listed as finished recordings.</p><script src="/voice.js"></script></html>)HTML";
constexpr char VOICE_JS[]=R"JS(
'use strict';
const $=id=>document.getElementById(id),token=document.querySelector('meta[name="paper-token"]').content;
async function req(url,body){const r=await fetch(url,body?{method:'POST',headers:{'X-Paper-Token':token},body:new URLSearchParams(body)}:{});const d=await r.json();if(!r.ok)throw Error(d.error||'Request failed');return d;}
async function poll(){try{const d=await req('/api/voice');$('status').textContent=`${d.state}\n${d.seconds} seconds · peak ${d.peak}\n${d.message}\n${d.path}`;$('start').disabled=d.active;$('stop').disabled=!d.active;}catch(e){$('status').textContent=e.message;}}
for(const action of ['start','stop'])$(action).onclick=async()=>{try{await req('/api/voice',{action});await poll();}catch(e){$('status').textContent=e.message;}};
$('clock').onclick=async()=>{try{await req('/api/clock',{epoch:String(Math.floor(Date.now()/1000))});$('status').textContent='Clock set to UTC.';}catch(e){$('status').textContent=e.message;}};
$('month').value=new Date().toISOString().slice(0,7);
$('list').onclick=async()=>{try{const d=await req('/api/recordings?month='+encodeURIComponent($('month').value));$('files').replaceChildren();for(const path of d.files){const li=document.createElement('li'),a=document.createElement('a'),audio=document.createElement('audio');a.textContent=path.split('/').pop();a.href='/api/recording?path='+encodeURIComponent(path);a.download=a.textContent;audio.controls=true;audio.preload='none';audio.src=a.href;li.append(a,audio);$('files').append(li);}}catch(e){$('status').textContent=e.message;}};
poll();setInterval(poll,1500);
)JS";
