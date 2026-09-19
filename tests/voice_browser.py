"""Offline Chromium check of the actual embedded voice-note UI."""
import argparse
from pathlib import Path
from urllib.parse import urlparse, parse_qs
from playwright.sync_api import sync_playwright

parser=argparse.ArgumentParser();parser.add_argument('--chromium',required=True);args=parser.parse_args()
source=Path('include/voice_page.h').read_text()
html=source.split('R"HTML(')[1].split(')HTML"')[0].replace('{{TOKEN}}','test-token')
js=source.split('R"JS(')[1].split(')JS"')[0]
state=dict(active=False,state='Idle',seconds=0,peak=0,message='Ready',path='')
note='/recordings/2026/09/20260919_134500_1234abcd.wav'
actions=[];errors=[]
def serve(route):
    r=route.request;p=urlparse(r.url).path
    if p=='/voice':route.fulfill(body=html,content_type='text/html')
    elif p=='/voice.js':route.fulfill(body=js,content_type='text/javascript')
    elif p=='/api/voice':
        if r.method=='POST':
            assert r.headers['x-paper-token']=='test-token'
            a=parse_qs(r.post_data)['action'][0];actions.append(a)
            state.update(active=a=='start',state='Recording' if a=='start' else 'Idle',path=note)
        route.fulfill(json=state)
    elif p=='/api/clock':
        assert r.headers['x-paper-token']=='test-token'
        assert int(parse_qs(r.post_data)['epoch'][0])>1704067200
        actions.append('clock');route.fulfill(json={'ok':True})
    elif p=='/api/recordings':route.fulfill(json={'files':[note]})
    else:route.fulfill(status=404,body='Not found')
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=args.chromium,headless=True,args=['--no-sandbox'])
    page=browser.new_page(viewport={'width':390,'height':844})
    page.route('http://paper.test/**',serve);page.on('pageerror',lambda e:errors.append(str(e)))
    page.goto('http://paper.test/voice',wait_until='networkidle')
    assert page.locator('#stop').is_disabled()
    page.locator('#start').click();page.wait_for_function("document.getElementById('start').disabled")
    assert not page.locator('#stop').is_disabled()
    page.locator('#stop').click();page.wait_for_function("!document.getElementById('start').disabled")
    page.locator('#clock').click();page.wait_for_function("document.getElementById('status').textContent.includes('Clock set')")
    page.locator('#list').click();page.wait_for_selector('#files audio')
    assert page.locator('audio').get_attribute('preload')=='none'
    assert page.locator('#files a').get_attribute('download')==note.split('/')[-1]
    assert actions==['start','stop','clock']
    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
    assert not errors,errors
    browser.close()
print('Voice browser: explicit start/stop, authenticated clock sync, lazy playback/download and mobile layout passed.')
