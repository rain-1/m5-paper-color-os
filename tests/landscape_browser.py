"""Real canvas rotation and packed-upload checks against mocked device routes."""
import argparse
import base64
from pathlib import Path
from urllib.parse import urlparse, parse_qs
from playwright.sync_api import sync_playwright

parser=argparse.ArgumentParser()
parser.add_argument('--chromium',required=True)
args=parser.parse_args()
source=Path('include/gallery.h').read_text()
html=source.split('R"HTML(')[1].split(')HTML"')[0].replace('{{TOKEN}}','abc123')
script=source.split('R"JS(')[1].split(')JS"')[0]
uploads=[]
display_queued=True
card_mounted=True
temporary_uploads=[]
thumb=bytes([80,54,84,49,80,0,120,0,0,0,0,0,0,0,0,0])+bytes([0x34])*4800
def serve(route):
    req=route.request;url=urlparse(req.url)
    if url.path=='/gallery':route.fulfill(body=html,content_type='text/html')
    elif url.path=='/converter.js':route.fulfill(body=script,content_type='text/javascript')
    elif url.path=='/api/card':route.fulfill(json=dict(mounted=card_mounted))
    elif url.path=='/api/images':route.fulfill(json=dict(files=['/pictures/2026/09/20260916_143025_deadbeef.p6']))
    elif url.path=='/api/thumbnail':route.fulfill(body=thumb,content_type='application/octet-stream')
    elif url.path=='/api/display':route.fulfill(status=202,json=dict(queued=True))
    elif url.path=='/api/upload':
        assert req.headers['x-paper-token']=='abc123'
        marker=parse_qs(url.query)['orientation'][0]
        packed=req.post_data_buffer.split(b'\r\n\r\n',1)[1].rsplit(b'\r\n--',1)[0]
        if parse_qs(url.query).get('target')==['display']:
            temporary_uploads.append(packed)
            route.fulfill(status=202,json=dict(displayQueued=True,saved=False))
            return
        uploads.append((marker,packed))
        route.fulfill(status=201,json=dict(path=f'/pictures/2026/09/20260916_143025_{marker}_deadbeef.p6',displayQueued=display_queued))
    else:route.fulfill(status=404)
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=args.chromium,headless=True,args=['--no-sandbox'])
    page=browser.new_page(viewport=dict(width=390,height=844));errors=[]
    page.on('pageerror',lambda e:errors.append(str(e)))
    page.route('http://paper.test/**',serve)
    page.goto('http://paper.test/gallery',wait_until='networkidle')
    assert not page.locator('#landscape').is_checked()
    assert page.locator('#matching').input_value()=='oklab'
    page.locator('#files canvas').scroll_into_view_if_needed()
    page.wait_for_function("document.querySelector('#files canvas').getContext('2d').getImageData(0,0,1,1).data[0]===255 && document.querySelector('#files canvas').getContext('2d').getImageData(0,0,1,1).data[1]===0")
    assert page.locator('#files canvas').get_attribute('width')=='80'
    png=page.evaluate("""()=>{const c=document.createElement('canvas');c.width=800;c.height=200;const x=c.getContext('2d');x.fillStyle='red';x.fillRect(0,0,400,200);x.fillStyle='blue';x.fillRect(400,0,400,200);return c.toDataURL().split(',')[1];}""")
    page.locator('#file').set_input_files(dict(name='wide.png',mimeType='image/png',buffer=base64.b64decode(png)))
    page.wait_for_function("!document.getElementById('upload').disabled")
    page.select_option('#dither','no')
    def pixel(x,y):return page.evaluate('([x,y])=>Array.from(document.getElementById("preview").getContext("2d").getImageData(x,y,1,1).data)',[x,y])
    red,blue,white=[255,0,0,255],[0,0,255,255],[255,255,255,255]
    assert pixel(100,300)==red and pixel(300,300)==blue and pixel(200,20)==white
    page.locator('#saturation').fill('0')
    page.locator('#saturation').dispatch_event('change')
    assert pixel(300,300)!=blue
    page.locator('#neutral').click()
    assert pixel(100,300)==red
    page.locator('#contrast').fill('50')
    page.locator('#contrast').dispatch_event('change')
    assert pixel(200,20)==white
    page.locator('#neutral').click()
    page.select_option('#matching','rgb')
    assert page.locator('#saturation').is_disabled()
    assert pixel(100,300)==red
    page.select_option('#matching','oklab')
    assert not page.locator('#saturation').is_disabled()
    page.locator('#landscape').check()
    assert pixel(200,20)==red and pixel(200,580)==blue and pixel(20,300)==white
    page.locator('#upload').click()
    page.wait_for_function("document.getElementById('message').textContent.includes('_l_deadbeef')")
    assert 'Display queued' in page.locator('#message').inner_text()
    assert page.locator('#files li').first.locator('canvas').count()==1
    marker,packed=uploads[-1]
    assert marker=='l' and len(packed)==120016
    def index(x,y):
        n=y*400+x;byte=packed[16+n//2];return byte&15 if n&1 else byte>>4
    assert index(200,20)==3 and index(200,580)==4 and index(20,300)==1
    # Toggle repeatedly: no accumulated rotation, stale conversion or wrong marker.
    page.locator('#landscape').uncheck()
    page.locator('#landscape').check()
    page.locator('#landscape').uncheck()
    assert pixel(100,300)==red and pixel(300,300)==blue and pixel(200,20)==white
    display_queued=False
    page.locator('#upload').click()
    page.wait_for_function("document.getElementById('message').textContent.includes('_p_deadbeef')")
    assert uploads[-1][0]=='p' and uploads[-1][1]!=packed
    assert 'NOT displayed' in page.locator('#message').inner_text()
    assert page.locator('#files button').count()==3 # new rows plus legacy path
    page.locator('#files li').first.locator('button').click()
    page.wait_for_function("document.getElementById('message').textContent.includes('Picture queued')")
    page.locator('#displayOnly').click()
    page.wait_for_function("document.getElementById('message').textContent.includes('Not saved to SD')")
    assert temporary_uploads[-1]==uploads[-1][1]
    assert page.locator('#files button').count()==3
    card_mounted=False
    page.reload(wait_until='networkidle')
    assert 'Display only still works' in page.locator('#card').inner_text()
    assert page.locator('#displayOnly').is_disabled()
    page.locator('#file').set_input_files(dict(name='wide.png',mimeType='image/png',buffer=base64.b64decode(png)))
    page.wait_for_function("!document.getElementById('displayOnly').disabled")
    page.locator('#displayOnly').click()
    page.wait_for_function("document.getElementById('message').textContent.includes('Not saved to SD')")
    assert len(temporary_uploads)==2 and len(temporary_uploads[-1])==120016
    assert page.locator('#files li').count()==0
    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth'), page.evaluate("Array.from(document.querySelectorAll('*')).filter(e=>e.getBoundingClientRect().right>innerWidth).map(e=>[e.tagName,e.id,e.getBoundingClientRect().right])")
    assert not errors,errors
    browser.close()
print('Landscape: clockwise pixels, aspect fit, borders, repeated toggles, packed uploads, orientation markers and mobile layout passed.')
