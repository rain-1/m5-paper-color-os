"""Generate a native-palette red/text comparison, with no second quantization.

Usage: python scripts/red_chart.py /tmp/paper-red-chart
Requires Pillow. Produces a PNG preview and the exact packed device image.
"""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

parser=argparse.ArgumentParser()
parser.add_argument('output',type=Path,help='Output path without extension')
args=parser.parse_args()
palette=[(0,0,0),(255,255,255),(255,255,0),(255,0,0),(0,0,255),(0,255,0)]
image=Image.new('P',(400,600),1)
image.putpalette([v for c in palette for v in c]+[0]*(768-18))
draw=ImageDraw.Draw(image)
draw.fontmode='1'  # Crisp text: only native black/white, no hidden gray pixels.
font_dir=Path('/usr/share/fonts/truetype/dejavu')
title=ImageFont.truetype(str(font_dir/'DejaVuSans-Bold.ttf'),19)
label=ImageFont.truetype(str(font_dir/'DejaVuSans.ttf'),13)
text=ImageFont.truetype(str(font_dir/'DejaVuSans-Bold.ttf'),19)
draw.text((10,7),'Which red reads best?',font=title,fill=0)
draw.text((10,32),'Same native inks / normal refresh / A–H',font=label,fill=0)
bayer=((0,8,2,10),(12,4,14,6),(3,11,1,9),(15,7,13,5))
samples=[('A  Pure red',16,1),('B  75% red + white',12,1),
         ('C  50% red + white',8,1),('D  25% red + white',4,1),
         ('E  75% red + yellow',12,2),('F  50% red + yellow',8,2),
         ('G  25% red + yellow',4,2),('H  Pure yellow',0,2)]
for i,(name,red_count,other) in enumerate(samples):
    x=10+(i%2)*195;y=54+(i//2)*128
    draw.text((x,y),name,font=label,fill=0)
    for py in range(y+20,y+112):
        for px in range(x,x+185):
            image.putpixel((px,py),3 if bayer[py%4][px%4]<red_count else other)
    assert sum(image.getpixel((px,py))==3 for py in range(y+24,y+28) for px in range(x+4,x+8))==red_count
    draw.rectangle((x,y+20,x+184,y+111),outline=0)
    draw.text((x+10,y+29),'Black text',font=text,fill=0)
    draw.text((x+10,y+68),'White text',font=text,fill=1)
draw.text((10,564),'Choose a letter and black or white text.',font=label,fill=0)
draw.text((10,581),'Mixtures are small dots, not voltage changes.',font=label,fill=0)
pixels=image.tobytes()
assert len(pixels)==240000 and max(pixels)<=5
packed=bytearray(b'P6I1\x90\x01\x58\x02'+bytes(8))
packed.extend((pixels[i]<<4)|pixels[i+1] for i in range(0,len(pixels),2))
assert len(packed)==120016
args.output.with_suffix('.p6').write_bytes(packed)
image.convert('RGB').save(args.output.with_suffix('.png'))
print('Generated',args.output.with_suffix('.p6'),'and PNG preview.')
