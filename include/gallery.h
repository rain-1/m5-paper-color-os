#pragma once
constexpr char GALLERY_HTML[] = R"HTML(<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><meta name="paper-token" content="{{TOKEN}}">
<title>Paper OS · Pictures</title><style>
*{box-sizing:border-box}body{font:17px system-ui;background:#f4f1e8;color:#19231e;margin:0}main{max-width:850px;margin:auto;padding:30px}
h1{font-size:44px;letter-spacing:-2px}section{padding:22px 0;border-top:2px solid #195d3b}button,input,select{font:inherit;padding:10px;margin:8px 0;max-width:100%}
button{background:#195d3b;color:white;border:0;border-radius:6px;cursor:pointer;overflow-wrap:anywhere}button:disabled{opacity:.5}canvas{display:block;width:200px;height:300px;background:white;border:1px solid #999;margin:15px 0}label{display:block}#message{min-height:3em;white-space:pre-wrap;overflow-wrap:anywhere}li{margin:8px 0;overflow-wrap:anywhere}a{color:#195d3b}small{color:#526158}
</style><main><nav>Paper OS / <a href="/books">Books</a> / <a href="/device">Device lab &amp; updates</a></nav><h1>Put something<br>on paper.</h1>
<p id="card">Checking SD card…</p><section><label>Choose a JPEG or PNG <input id="file" type="file" accept="image/jpeg,image/png"></label>
<small>Maximum 12 MB, 24 megapixels, 12,000 pixels per side. Conversion stays in this browser.</small>
<label>Colour matching <select id="matching"><option value="oklab">Perceptual (OKLab)</option><option value="rgb">Original RGB (comparison)</option></select></label>
<label>Saturation <input id="saturation" type="range" min="0" max="200" step="5" value="100"> <output id="saturationValue">100%</output></label>
<label>Contrast <input id="contrast" type="range" min="50" max="200" step="5" value="100"> <output id="contrastValue">100%</output></label>
<button id="neutral" type="button">Reset adjustments</button><small>Adjustments apply to perceptual mode; 100% is neutral. These change the picture, not the panel. Palette colours are not calibrated to this screen.</small>
<label>Dithering <select id="dither"><option value="yes">Floyd–Steinberg dithering</option><option value="no">Solid six colours</option></select></label>
<label><input id="landscape" type="checkbox"> Landscape · rotate 90° clockwise</label>
<canvas id="preview" width="400" height="600"></canvas><small>Fits the whole image with white borders. Preview colours are approximate.</small><br>
<button id="upload" disabled>Save to SD &amp; display</button><p id="message" role="status" aria-live="polite"></p></section>
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
function fittedSize(w,h,landscape=false) {
  checkDimensions(w,h);
  const scale=landscape?Math.min(400/h,600/w):Math.min(400/w,600/h);
  return [w*scale,h*scale];
}
function thumbnailPixels(bytes) {
  const header=[80,54,84,49,80,0,120,0,0,0,0,0,0,0,0,0];
  if(bytes.length!==4816||!header.every((v,i)=>bytes[i]===v))throw Error('Invalid thumbnail');
  const rgba=new Uint8ClampedArray(80*120*4);
  for(let i=0;i<80*120;i++){const c=(bytes[16+(i>>1)]>>((i&1)?0:4))&15;if(c>5)throw Error('Invalid thumbnail colour');rgba.set([...palette[c],255],i*4);}
  return rgba;
}
// Björn Ottosson's public-domain sRGB/OKLab matrices:
// https://bottosson.github.io/posts/oklab/ (2021-01-25 revision).
function oklab(rgb) {
  const [r,g,b]=rgb.map(v=>{v/=255;return v<=0.04045?v/12.92:((v+0.055)/1.055)**2.4;});
  const l=Math.cbrt(.4122214708*r+.5363325363*g+.0514459929*b);
  const m=Math.cbrt(.2119034982*r+.6806995451*g+.1073969566*b);
  const s=Math.cbrt(.0883024619*r+.2817188376*g+.6299787005*b);
  return [.2104542553*l+.7936177850*m-.0040720468*s,
    1.9779984951*l-2.4285922050*m+.4505937099*s,
    .0259040371*l+.7827717662*m-.8086757660*s];
}
const perceptualPalette=palette.map(oklab);
function treatedLab(rgb,saturation,contrast) {
  const lab=oklab(rgb),l=Math.max(0,Math.min(1,lab[0]));
  // Symmetric lightness curve keeps black and white endpoints (and borders).
  lab[0]=l<=.5?.5*(2*l)**contrast:1-.5*(2*(1-l))**contrast;
  lab[1]*=saturation;lab[2]*=saturation;return lab;
}
function quantize(rgba,w,h,dither,options={}) {
  const perceptual=options.matching==='oklab',colours=perceptual?perceptualPalette:palette;
  const saturation=options.saturation??1,contrast=options.contrast??1;
  if(!Number.isFinite(saturation)||saturation<0||saturation>2||!Number.isFinite(contrast)||contrast<.5||contrast>2)throw Error('Invalid colour adjustment');
  const work=new Float32Array(w*h*3), packed=new Uint8Array(16+w*h/2);
  packed.set([80,54,73,49,144,1,88,2]);
  for(let i=0;i<w*h;i++) {
    const rgb=[rgba[i*4],rgba[i*4+1],rgba[i*4+2]];
    work.set(perceptual?treatedLab(rgb,saturation,contrast):rgb,i*3);
  }
  for(let y=0;y<h;y++) for(let x=0;x<w;x++) {
    const i=y*w+x; let best=0,score=Infinity;
    for(let p=0;p<6;p++) {
      let distance=0;
      for(let c=0;c<3;c++) distance+=(Math.max(perceptual&&c?-1:0,Math.min(perceptual?1:255,work[i*3+c]))-colours[p][c])**2;
      if(distance<score){score=distance;best=p;}
    }
    packed[16+(i>>1)]|=best<<((i&1)?0:4);
    for(let c=0;c<3;c++) {
      const limit=perceptual?1:255;
      const e=Math.max(-limit,Math.min(limit,work[i*3+c]-colours[best][c]));
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
if(typeof module!=='undefined') module.exports={dimensions,checkDimensions,fittedSize,thumbnailPixels,oklab,treatedLab,quantize};
if(typeof document!=='undefined') {
  const $=id=>document.getElementById(id), token=document.querySelector('meta[name="paper-token"]').content;
  const ctx=$('preview').getContext('2d'); let converted=null, convertedOrientation='p', image=null, generation=0, uploading=false;
  const say=text=>$('message').textContent=text;
  let listGeneration=0,thumbQueue=[],thumbRunning=false;
  const observer=new IntersectionObserver(entries=>{for(const e of entries)if(e.isIntersecting){observer.unobserve(e.target);thumbQueue.push(e.target);pumpThumbnails();}},{rootMargin:'150px'});
  async function pumpThumbnails(){
    if(thumbRunning)return;thumbRunning=true;
    try{while(thumbQueue.length){const c=thumbQueue.shift();if(!c.isConnected)continue;
      try{const r=await fetch('/api/thumbnail?path='+encodeURIComponent(c.dataset.path));if(!r.ok)throw Error('Preview unavailable; load month again after refresh.');const rgba=thumbnailPixels(new Uint8Array(await r.arrayBuffer()));if(c.isConnected)c.getContext('2d').putImageData(new ImageData(rgba,80,120),0,0);}
      catch(e){c.title=e.message;const x=c.getContext('2d');x.fillStyle='#526158';x.font='11px sans-serif';x.fillText('Reload preview',3,60);}
    }}finally{thumbRunning=false;}
  }
  function pictureRow(path,localPreview=null){
    const li=document.createElement('li'),button=document.createElement('button'),thumb=document.createElement('canvas');
    li.dataset.path=path;li.style.display='flex';li.style.alignItems='center';li.style.gap='12px';
    thumb.width=80;thumb.height=120;thumb.style.cssText='width:60px;height:90px;flex:none;margin:0';thumb.dataset.path=path;thumb.setAttribute('role','img');thumb.setAttribute('aria-label','Preview of '+path.split('/').pop());
    thumb.getContext('2d').fillStyle='white';thumb.getContext('2d').fillRect(0,0,80,120);
    if(localPreview){thumb.getContext('2d').imageSmoothingEnabled=false;thumb.getContext('2d').drawImage(localPreview,0,0,80,120);}else observer.observe(thumb);
    button.style.minWidth='0';button.textContent='Display '+path.split('/').pop();
    button.onclick=async()=>{try{await request('/api/display',{method:'POST',body:new URLSearchParams({path})});say('Picture queued. Allow time for the screen to refresh.');}catch(e){say(e.message);}};
    li.append(thumb,button);return li;
  }
  async function request(url,options={}) {
    const response=await fetch(url,{...options,headers:{...options.headers,'X-Paper-Token':token}});
    const data=await response.json(); if(!response.ok)throw Error(data.error||'Request failed'); return data;
  }
  function convert() {
    converted=null;$('upload').disabled=true;
    if(!image)return;
    const landscape=$('landscape').checked;
    ctx.fillStyle='white';ctx.fillRect(0,0,400,600);
    const [w,h]=fittedSize(image.naturalWidth,image.naturalHeight,landscape);
    ctx.save();
    try {ctx.translate(200,300);if(landscape)ctx.rotate(Math.PI/2);ctx.drawImage(image,-w/2,-h/2,w,h);}
    finally {ctx.restore();}
    const pixels=ctx.getImageData(0,0,400,600);
    converted=quantize(pixels.data,400,600,$('dither').value==='yes',{matching:$('matching').value,saturation:Number($('saturation').value)/100,contrast:Number($('contrast').value)/100});
    convertedOrientation=landscape?'l':'p';
    ctx.putImageData(pixels,0,0);$('upload').disabled=uploading;
    say('Ready ('+(landscape?'landscape, rotated clockwise':'portrait')+'). Only the converted 120 KB picture will be uploaded. Filename marker: '+convertedOrientation+'.');
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
  $('landscape').onchange=()=>{try{convert();}catch(e){say(e.message);}};
  function treatmentChanged(){
    for(const id of ['saturation','contrast']){$(id).disabled=$('matching').value==='rgb';$(id+'Value').value=$(id).value+'%';}
    try{convert();}catch(e){say(e.message);}
  }
  $('matching').onchange=treatmentChanged;
  // Update labels while dragging; convert once on release to keep mobile responsive.
  for(const id of ['saturation','contrast']){$(id).oninput=()=>$(id+'Value').value=$(id).value+'%';$(id).onchange=treatmentChanged;}
  $('neutral').onclick=()=>{$('saturation').value=100;$('contrast').value=100;treatmentChanged();};
  async function list() {
    const revision=++listGeneration;
    try {
      const data=await request('/api/images?month='+encodeURIComponent($('month').value));
      if(revision!==listGeneration)return;
      observer.disconnect();thumbQueue=[];
      $('files').replaceChildren();
      for(const path of data.files.sort().reverse()) {
        $('files').append(pictureRow(path));
      }
      if(!data.files.length)$('files').textContent='No pictures in this month yet.';
    }catch(e){say(e.message);}
  }
  $('upload').onclick=async()=>{
    if(!converted||uploading)return;uploading=true;$('upload').disabled=true;
    try {
      const now=new Date(),pad=n=>String(n).padStart(2,'0');
      const date=`${now.getFullYear()}${pad(now.getMonth()+1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}${pad(now.getSeconds())}`;
      const body=new FormData();body.append('image',new Blob([converted],{type:'application/octet-stream'}),'picture.p6');
      const savedPreview=document.createElement('canvas');savedPreview.width=400;savedPreview.height=600;savedPreview.getContext('2d').drawImage($('preview'),0,0);
      say('Saving and checking the SD card…');
      const data=await request('/api/upload?date='+date+'&orientation='+convertedOrientation,{method:'POST',body});
      ++listGeneration;const month=`${now.getFullYear()}-${pad(now.getMonth()+1)}`;
      if($('month').value!==month||!$('files').querySelector('li')){$('files').replaceChildren();observer.disconnect();thumbQueue=[];}
      $('month').value=month;
      for(const row of $('files').querySelectorAll('li'))if(row.dataset.path===data.path)row.remove();
      $('files').prepend(pictureRow(data.path,savedPreview));
      say('Saved '+data.path+(data.displayQueued?'. Display queued; allow time for the screen to refresh.':'. Saved, but NOT displayed: the reader is busy or display is blocked. Use its Display button when ready; no need to upload again.'));
    }catch(e){say(e.message);}finally{uploading=false;$('upload').disabled=!converted;}
  };
  const now=new Date();$('month').value=`${now.getFullYear()}-${String(now.getMonth()+1).padStart(2,'0')}`;
  $('refresh').onclick=list;
  request('/api/card').then(data=>{$('card').textContent=data.mounted?'SD card ready.':'SD card not mounted. Insert a FAT32 card and restart.';if(data.mounted)list();}).catch(e=>say(e.message));
}
)JS";
