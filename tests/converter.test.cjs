const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('include/gallery.h','utf8').split('R"JS(')[1].split(')JS"')[0];
const context={module:{exports:{}},Uint8Array,Float32Array,DataView};vm.runInNewContext(source,context);
const {dimensions,checkDimensions,fittedSize,quantize}=context.module.exports;
assert.deepEqual([...fittedSize(800,200,false)],[400,100]);
assert.deepEqual([...fittedSize(800,200,true)],[600,150]);
fittedSize(400,600,true).forEach((x,i)=>assert(Math.abs(x-[400*2/3,400][i])<1e-9));
assert.throws(()=>fittedSize(0,100,true));
const png=new Uint8Array(24);png.set([137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82]);
const v=new DataView(png.buffer);v.setUint32(16,400);v.setUint32(20,600);
assert.deepEqual([...dimensions(png)],[400,600]);
const jpeg=new Uint8Array([255,216,255,192,0,8,8,2,88,1,144,1]);
assert.deepEqual([...dimensions(jpeg)],[400,600]);
assert.throws(()=>dimensions(new Uint8Array([255,216,255,192,0,255])));
assert.throws(()=>checkDimensions(12001,1));assert.throws(()=>checkDimensions(6000,6000));assert.throws(()=>checkDimensions(0,600));
checkDimensions(6000,4000);
const rgba=new Uint8ClampedArray(400*600*4);
for(let i=0;i<400*600;i++){rgba.set(i%2?[255,255,255,255]:[0,0,0,255],i*4);}
const packed=quantize(rgba,400,600,false);
assert.equal(packed.length,120016);assert.equal(packed[16],0x01);assert.equal(packed[packed.length-1],0x01);
const gray=new Uint8ClampedArray(400*600*4).fill(128);
const dithered=quantize(gray,400,600,true);
assert(dithered.subarray(16).some(x=>x!==dithered[16]));
for(const b of dithered.subarray(16)){assert((b>>4)<=5);assert((b&15)<=5);}
new vm.Script(fs.readFileSync('include/device_page.h','utf8').split('R"JS(')[1].split(')JS"')[0]);
console.log('Converter: dimensions, size limits, packing, dithering and script syntax passed.');
