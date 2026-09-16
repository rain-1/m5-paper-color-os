#pragma once
constexpr char GALLERY_HTML[] = R"HTML(<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><meta name="paper-token" content="{{TOKEN}}">
<title>Paper OS · Pictures</title><style>
*{box-sizing:border-box}body{font:17px system-ui;background:#f4f1e8;color:#19231e;margin:0}main{max-width:850px;margin:auto;padding:30px}
h1{font-size:44px;letter-spacing:-2px}section{padding:22px 0;border-top:2px solid #195d3b}button,input,select{font:inherit;padding:10px;margin:8px 0;max-width:100%}
button{background:#195d3b;color:white;border:0;border-radius:6px;cursor:pointer}button:disabled{opacity:.5}canvas{display:block;width:200px;height:300px;background:white;border:1px solid #999;margin:15px 0}label{display:block}#message{min-height:3em;white-space:pre-wrap}li{margin:8px 0;overflow-wrap:anywhere}a{color:#195d3b}small{color:#526158}
</style><main><nav>Paper OS / <a href="/device">Device lab &amp; updates</a></nav><h1>Put something<br>on paper.</h1>
<p id="card">Checking SD card…</p><section><label>Choose a JPEG or PNG <input id="file" type="file" accept="image/jpeg,image/png"></label>
<small>Maximum 12 MB, 24 megapixels, 12,000 pixels per side. Conversion stays in this browser.</small>
<label>Colour treatment <select id="dither"><option value="yes">Floyd–Steinberg dithering</option><option value="no">Solid six colours</option></select></label>
<canvas id="preview" width="400" height="600"></canvas><small>Fits the whole image with white borders. Preview colours are approximate.</small><br>
<button id="upload" disabled>Save to SD card</button><p id="message" role="status" aria-live="polite"></p></section>
<section><h2>Your pictures</h2><input id="month" type="month"><button id="refresh">Load month</button><p>Showing up to 100 files per month. Select a picture to display it.</p><ul id="files"></ul></section></main><script src="/converter.js"></script></html>)HTML";

constexpr char CONVERTER_JS[] = R"JS(
'use strict';
const palette=[[0,0,0],[255,255,255],[255,255,0],[255,0,0],[0,0,255],[0,255,0]];
function dimensions(bytes) {
  const v=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
  if(bytes.length>=24 && [137,80,78,71,13,10,26,10].every((x,i)=>bytes[i]===x) &&
     v.getUint32(8)===13 && v.getUint32(12)===0x49484452) return [v.getUint32(16),v.getUint32(20)];
  if(bytes[0]===255 && bytes[1]===216) {
    let i=2;
    while(i+4<=bytes.length) {
      if(bytes[i++]!==255) break;
      while(bytes[i]===255) i++;
      const marker=bytes[i++];
      if(marker===0xda || marker===0xd9) break;
      if(marker===1 || (marker>=0xd0 && marker<=0xd7)) continue;
      if(i+2>bytes.length) break;
      const length=v.getUint16(i);
      if(length<2 || i+length>bytes.length) break;
      if([0xc0,0xc1,0xc2].includes(marker) && length>=8) return [v.getUint16(i+5),v.getUint16(i+3)];
      i+=length;
    }
  }
  throw Error('Use a supported JPEG or PNG image.');
}
function checkDimensions(w,h) {
  if(!w || !h || w>12000 || h>12000 || w*h>24000000) throw Error('Image too large: maximum 24 megapixels and 12,000 pixels per side.');
}
function quantize(rgba,w,h,dither) {
  const work=new Float32Array(w*h*3), packed=new Uint8Array(16+w*h/2);
  packed.set([80,54,73,49,144,1,88,2]);
  for(let i=0;i<w*h;i++) for(let c=0;c<3;c++) work[i*3+c]=rgba[i*4+c];
  for(let y=0;y<h;y++) for(let x=0;x<w;x++) {
    const i=y*w+x; let best=0,score=Infinity;
    for(let p=0;p<6;p++) {
      let distance=0;
      for(let c=0;c<3;c++) distance+=(Math.max(0,Math.min(255,work[i*3+c]))-palette[p][c])**2;
      if(distance<score){score=distance;best=p;}
    }
    packed[16+(i>>1)]|=best<<((i&1)?0:4);
    for(let c=0;c<3;c++) {
      const e=Math.max(-255,Math.min(255,work[i*3+c]-palette[best][c]));
      rgba[i*4+c]=palette[best][c];
      if(dither) {
        if(x+1<w)work[(i+1)*3+c]+=e*7/16;
        if(y+1<h) {
          if(x)work[(i+w-1)*3+c]+=e*3/16;
          work[(i+w)*3+c]+=e*5/16;
          if(x+1<w)work[(i+w+1)*3+c]+=e/16;
        }
      }
    }
    rgba[i*4+3]=255;
  }
  return packed;
}
if(typeof module!=='undefined') module.exports={dimensions,checkDimensions,quantize};
if(typeof document!=='undefined') {
  const $=id=>document.getElementById(id), token=document.querySelector('meta[name="paper-token"]').content;
  const ctx=$('preview').getContext('2d'); let converted=null, image=null, generation=0;
  const say=text=>$('message').textContent=text;
  async function request(url,options={}) {
    const response=await fetch(url,{...options,headers:{...options.headers,'X-Paper-Token':token}});
    const data=await response.json(); if(!response.ok)throw Error(data.error||'Request failed'); return data;
  }
  function convert() {
    if(!image)return;
    ctx.fillStyle='white';ctx.fillRect(0,0,400,600);
    const scale=Math.min(400/image.naturalWidth,600/image.naturalHeight);
    const w=image.naturalWidth*scale,h=image.naturalHeight*scale;
    ctx.drawImage(image,(400-w)/2,(600-h)/2,w,h);
    const pixels=ctx.getImageData(0,0,400,600);
    converted=quantize(pixels.data,400,600,$('dither').value==='yes');
    ctx.putImageData(pixels,0,0);$('upload').disabled=false;
    say('Ready. Only the converted 120 KB picture will be uploaded.');
  }
  $('file').onchange=async()=>{
    const revision=++generation;converted=null;image=null;$('upload').disabled=true;
    try {
      const file=$('file').files[0];if(!file)return;
      if(file.size>12*1024*1024)throw Error('Choose an image smaller than 12 MB.');
      const bytes=new Uint8Array(await file.arrayBuffer());
      checkDimensions(...dimensions(bytes));
      const url=URL.createObjectURL(file),next=new Image();
      try { await new Promise((resolve,reject)=>{next.onload=resolve;next.onerror=()=>reject(Error('Image could not be decoded.'));next.src=url;}); }
      finally {URL.revokeObjectURL(url);}
      checkDimensions(next.naturalWidth,next.naturalHeight);
      if(revision!==generation)return;
      image=next;convert();
    }catch(e){if(revision===generation)say(e.message);}
  };
  $('dither').onchange=()=>{try{convert();}catch(e){say(e.message);}};
  async function list() {
    try {
      const data=await request('/api/images?month='+encodeURIComponent($('month').value));
      $('files').replaceChildren();
      for(const path of data.files.sort().reverse()) {
        const li=document.createElement('li'),button=document.createElement('button');
        button.textContent='Display '+path.split('/').pop();
        button.onclick=async()=>{try{await request('/api/display',{method:'POST',body:new URLSearchParams({path})});say('Picture queued. Allow 30 seconds for the screen to refresh.');}catch(e){say(e.message);}};
        li.append(button);$('files').append(li);
      }
      if(!data.files.length)$('files').textContent='No pictures in this month yet.';
    }catch(e){say(e.message);}
  }
  $('upload').onclick=async()=>{
    if(!converted)return;$('upload').disabled=true;
    try {
      const now=new Date(),pad=n=>String(n).padStart(2,'0');
      const date=`${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
      const body=new FormData();body.append('image',new Blob([converted],{type:'application/octet-stream'}),'picture.p6');
      say('Saving and checking the SD card…');
      const data=await request('/api/upload?date='+date,{method:'POST',body});
      $('month').value=`${now.getFullYear()}-${pad(now.getMonth()+1)}`;
      await list();say('Saved '+data.path+'. Select it below to display.');
    }catch(e){say(e.message);}finally{$('upload').disabled=!converted;}
  };
  const now=new Date();$('month').value=`${now.getFullYear()}-${String(now.getMonth()+1).padStart(2,'0')}`;
  $('refresh').onclick=list;
  request('/api/card').then(data=>{$('card').textContent=data.mounted?'SD card ready.':'SD card not mounted. Insert a FAT32 card and restart.';if(data.mounted)list();}).catch(e=>say(e.message));
}
)JS";
