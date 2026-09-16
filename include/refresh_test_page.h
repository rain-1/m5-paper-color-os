#pragma once
constexpr char REFRESH_TEST_HTML[] = R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="paper-token" content="{{TOKEN}}"><title>Paper OS · Refresh experiment</title><style>
body{font:18px system-ui;background:#f4f1e8;color:#19231e;max-width:720px;margin:auto;padding:24px}button{font:inherit;padding:14px;margin:8px 8px 8px 0}pre{white-space:pre-wrap}a{color:#195d3b}
</style><a href="/device">Device lab</a> / <a href="/books">Books</a><h1>Refresh timing experiment</h1>
<p>This is an experimental <strong>full-screen</strong> refresh, not a partial update. It changes the panel clock for one chart only. Normal timing is restored after completion; nothing is enabled permanently.</p>
<p>Keep the device powered, at room temperature, and do not press power/reset while it refreshes. Image quality and long-term behaviour at the experimental timing are unverified.</p>
<ol><li>Run the normal chart. Wait until the status says complete and inspect it.</li><li>Tick the acknowledgement, then run one accelerated chart.</li><li>Check that the old left dot disappears, the right dot is solid, text is sharp, backgrounds are clean, and colours look comparable.</li><li>If it looks poor, run the normal chart again. Return to Books to continue reading at normal timing.</li></ol>
<label><input id="ack" type="checkbox"> I understand this changes panel timing experimentally.</label><p><button id="normal">1. Normal chart</button><button id="fast" disabled>2. Accelerated chart (one shot)</button></p>
<pre id="state">Checking device…</pre><p id="message" role="status"></p><p>If a timeout is reported, display commands are blocked until restart. Leave it powered and report the status; do not repeatedly retry.</p><script src="/refresh-test.js"></script></html>)HTML";
constexpr char REFRESH_TEST_JS[] = R"JS(
'use strict';
const $=id=>document.getElementById(id),token=document.querySelector('meta[name="paper-token"]').content;
let last=null,sending=false;
function controls(){const blocked=sending||!last||last.busy||last.faulted;$('normal').disabled=blocked;$('fast').disabled=blocked||!last.baselineMs||!$('ack').checked;}
async function poll(){try{const r=await fetch('/api/refresh-test');if(!r.ok)throw Error('Cannot read test status');last=await r.json();$('state').textContent=`${last.message}\nLast mode: ${last.mode}\nNormal total: ${last.baselineMs? (last.baselineMs/1000).toFixed(2)+' s':'not measured'}\nAccelerated total: ${last.fastMs?(last.fastMs/1000).toFixed(2)+' s':'not measured'}\nLast refresh BUSY interval: ${(last.busyMs/1000).toFixed(2)} s\nDefault timing restored: ${last.restored?'yes':'not confirmed'}\nReader/display request busy: ${last.busy?'yes':'no'}`;controls();}catch(e){$('message').textContent=e.message;last=null;controls();}}
async function run(mode){sending=true;controls();try{const r=await fetch('/api/refresh-test',{method:'POST',headers:{'X-Paper-Token':token},body:new URLSearchParams({mode,ack:$('ack').checked?'timing-experiment':''})});const data=await r.json();if(!r.ok)throw Error(data.error||'Test rejected');$('message').textContent='Queued. Watch the physical screen; keep power connected.';await poll();}catch(e){$('message').textContent=e.message;}finally{sending=false;controls();}}
$('normal').onclick=()=>run('normal');$('fast').onclick=()=>run('accelerated');$('ack').onchange=controls;poll();setInterval(poll,1000);
)JS";
