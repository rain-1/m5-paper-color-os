"""Exercise the embedded experiment UI against mocked hardware responses."""
import argparse
from pathlib import Path
from urllib.parse import urlparse, parse_qs
from playwright.sync_api import sync_playwright

parser=argparse.ArgumentParser()
parser.add_argument("--chromium",required=True)
args=parser.parse_args()
source=Path("include/refresh_test_page.h").read_text()
html=source.split('R"HTML(')[1].split(')HTML"')[0].replace("{{TOKEN}}","abc123")
script=source.split('R"JS(')[1].split(')JS"')[0]
state=dict(busy=False,faulted=False,restored=True,baselineMs=0,fastMs=0,busyMs=0,mode="none",message="Ready")
posts=[]
def serve(route):
    r=route.request
    path=urlparse(r.url).path
    if path=="/refresh-test":route.fulfill(body=html,content_type="text/html")
    elif path=="/refresh-test.js":route.fulfill(body=script,content_type="text/javascript")
    elif path=="/api/refresh-test" and r.method=="GET":route.fulfill(json=state)
    elif path=="/api/refresh-test" and r.method=="POST":
        assert r.headers['x-paper-token']=='abc123'
        posts.append(parse_qs(r.post_data))
        state['busy']=True
        route.fulfill(status=202,json=dict(queued=True))
    else:route.fulfill(status=404)
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path=args.chromium,headless=True,args=['--no-sandbox'])
    page=browser.new_page(viewport=dict(width=390,height=844));errors=[]
    page.on('pageerror',lambda e:errors.append(str(e)))
    page.route('http://paper.test/**',serve)
    page.goto('http://paper.test/refresh-test',wait_until='networkidle')
    assert page.locator('#normal').is_enabled() and page.locator('#fast').is_disabled()
    page.locator('#ack').check()
    assert page.locator('#fast').is_disabled() # baseline still required
    page.locator('#normal').click()
    page.wait_for_function("document.getElementById('normal').disabled")
    assert posts[-1]['mode']==['normal']
    state.update(busy=False,baselineMs=25000,busyMs=24000,mode='normal',message='Complete')
    page.wait_for_function("!document.getElementById('fast').disabled")
    page.locator('#fast').click()
    page.wait_for_function("document.getElementById('normal').disabled")
    assert posts[-1]==dict(mode=['accelerated'],ack=['timing-experiment'])
    state.update(busy=False,fastMs=10000,busyMs=9000,mode='accelerated')
    page.wait_for_function("document.getElementById('state').textContent.includes('10.00 s')")
    page.locator('#ack').uncheck()
    assert page.locator('#fast').is_disabled()
    state.update(faulted=True,restored=False,message='Timeout')
    page.wait_for_function("document.getElementById('normal').disabled")
    assert page.locator('#fast').is_disabled()
    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
    assert not errors,errors
    browser.close()
print('Experiment browser: baseline requirement, acknowledgement, one-shot requests, busy/fault locks, timings and mobile layout passed.')
