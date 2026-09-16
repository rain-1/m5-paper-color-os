#pragma once
constexpr char DEVICE_HTML[] = R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="paper-token" content="{{TOKEN}}"><title>Paper OS · Device lab</title><style>
body{font:17px system-ui;background:#f4f1e8;color:#19231e;max-width:800px;margin:auto;padding:28px}section{border-top:2px solid #195d3b;margin-top:28px;padding-top:12px}button,input{font:inherit;padding:12px;margin:6px;max-width:95%}button{background:#195d3b;color:white;border:0;border-radius:6px;cursor:pointer}pre{white-space:pre-wrap;line-height:1.6}a{color:#195d3b}#message{min-height:3em}button:disabled{opacity:.5}
</style><a href="/gallery">← Pictures</a><h1>Device lab</h1><p>With the screen upright and USB at the bottom: A is upper-left, B is lower-left, and C is on the top edge. Press them to watch the counters. A shows battery/status; hold B for Wi-Fi setup.</p>
<svg width="180" height="220" viewBox="0 0 180 220" role="img" aria-label="Front view: C at top; A then B down upper left; power near lower left; USB at bottom"><rect x="48" y="30" width="112" height="164" rx="8" fill="white" stroke="#195d3b"/><rect x="88" y="23" width="32" height="7" fill="#195d3b"/><text x="88" y="17">C</text><text x="4" y="64">A →</text><text x="4" y="91">B →</text><text x="0" y="169" font-size="12">PWR →</text><text x="89" y="210" font-size="12">USB</text><text x="69" y="119" font-size="13">SCREEN</text></svg>
<pre id="status">Reading device…</pre><section><h2>Try the hardware</h2><button data-action="led0">LED 1 · red</button><button data-action="led1">LED 2 · blue</button><button data-action="off">LEDs off</button><button data-action="tone">Play a short tone</button><p>LED tests stop automatically after five seconds. The speaker test plays a short, low-volume beep.</p></section>
<section><h2>Sound language</h2><label><input id="sounds" type="checkbox"> Enable event sounds (remembered on this device)</label><p>Rising notes mean success; falling notes mean failure. Two equal notes mean busy. Three rising notes mean you can release C: updates are unlocked.</p><button data-cue="saved">Preview saved</button><button data-cue="error">Preview error</button><button data-cue="busy">Preview busy</button><button data-cue="unlocked">Preview unlocked</button><button data-cue="updated">Preview update complete</button><p>Previews only play the cue; they do not unlock, save or restart anything. Muting keeps visual LED feedback.</p></section>
<section><h2>Firmware update</h2><p>Hold the top-edge button C until it beeps and the LEDs flash green (2.5 seconds), then release. Uploads are unlocked for two minutes. Choose a Paper OS <code>firmware.bin</code> built for this device. Keep the device powered until it restarts.</p>
<input id="firmware" type="file" accept=".bin"><button id="update" disabled>Upload firmware &amp; restart</button><p>The SD card and saved Wi-Fi settings are preserved. USB remains the recovery route if new firmware fails to boot.</p></section>
<p id="power">Checking update power requirements…</p>
<p id="message" role="status"></p><section><h2>Hardware inventory</h2><p>ESP32-S3 · 16 MB flash · 8 MB PSRAM · 400 × 600 six-colour e-paper · 2.4 GHz Wi-Fi · three user buttons · power/reset button · two RGB LEDs · 1 W speaker · microphone with echo-cancellation hardware · SHT40 temperature/humidity · RX8130 RTC · microSD · infrared emitter · Grove expansion · M5PM1 power management · 1250 mAh battery.</p>
<p>This lab exposes power, buttons, LEDs, speaker, environmental readings, RTC, memory and SD status. Microphone recording, infrared transmission, Grove controls, clock setting and sleep scheduling are planned.</p></section><script src="/device.js"></script></html>)HTML";
constexpr char DEVICE_JS[] = R"JS(
'use strict';
const $=id=>document.getElementById(id), token=document.querySelector('meta[name="paper-token"]').content;
let updating=false;
async function request(url,options={}){const r=await fetch(url,{...options,headers:{...options.headers,'X-Paper-Token':token}});const data=await r.json();if(!r.ok)throw Error(data.error||'Request failed');return data;}
async function poll(){
  if(updating)return;
  try{
    const s=await request('/api/device');
    $('status').textContent=`Firmware: ${s.version}\nBattery: ${s.battery}% (estimate), ${s.millivolts} mV\nCharging: ${s.charging}\nWi-Fi: ${s.ip} / ${s.rssi} dBm\nButton presses: A ${s.buttons[0]} · B ${s.buttons[1]} · C ${s.buttons[2]}\nTemperature: ${s.temperature===null?'unavailable':s.temperature+' °C'}\nHumidity: ${s.humidity===null?'unavailable':s.humidity+' %'}\nRTC: ${s.rtc} (not synchronised by this firmware)\nFree heap: ${s.heap} bytes\nFree PSRAM: ${s.psram} bytes\nUptime: ${s.uptime} s\nLast restart: ${s.resetReason}\nOTA: ${s.otaSeconds?'unlocked for '+s.otaSeconds+' s':'locked'}`;
    $('update').disabled=!s.otaSeconds || !s.otaPowerOk;
    $('power').textContent=`Input: ${s.inputMillivolts>0?s.inputMillivolts+' mV':'not detected / unavailable'}. Update power: ${s.otaPowerOk?'OK':'blocked'}. Requires 4.6–5.5 V input, or at least 30% battery and 3.6 V.`;
    $('sounds').checked=s.sounds;
  }catch(e){$('status').textContent=e.message;}
}
for(const button of document.querySelectorAll('[data-action]'))button.onclick=async()=>{try{await request('/api/test',{method:'POST',body:new URLSearchParams({action:button.dataset.action})});$('message').textContent='Test started.';}catch(e){$('message').textContent=e.message;}};
for(const button of document.querySelectorAll('[data-cue]'))button.onclick=async()=>{try{await request('/api/feedback',{method:'POST',body:new URLSearchParams({cue:button.dataset.cue})});$('message').textContent='Cue preview requested.';}catch(e){$('message').textContent=e.message;}};
$('sounds').onchange=async()=>{try{await request('/api/feedback',{method:'POST',body:new URLSearchParams({enabled:String($('sounds').checked)})});$('message').textContent='Sound preference saved.';}catch(e){$('message').textContent=e.message;}};
$('update').onclick=async()=>{const file=$('firmware').files[0];if(!file){$('message').textContent='Choose firmware.bin first.';return;}if(file.size<1024||file.size>0x640000){$('message').textContent='Firmware size is outside the supported range.';return;}updating=true;$('update').disabled=true;try{const body=new FormData();body.append('firmware',file);$('message').textContent='Uploading firmware. Keep the device powered…';await request('/api/update?size='+file.size,{method:'POST',body});$('message').textContent='Update verified. Restarting; reload this page in about 30 seconds.';}catch(e){$('message').textContent=e.message;updating=false;}};
poll();setInterval(poll,2000);
)JS";
