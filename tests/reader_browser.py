"""Offline Chromium integration test for the actual embedded reader page/scripts."""
import argparse
import json
from pathlib import Path
from urllib.parse import urlparse
from playwright.sync_api import sync_playwright

parser = argparse.ArgumentParser()
parser.add_argument("--chromium", required=True)
args = parser.parse_args()
source = Path("include/reader_page.h").read_text()
html = source.split('R"HTML(')[1].split(')HTML"')[0].replace("{{TOKEN}}", "123abc")
script = source.split('R"JS(')[1].split(')JS"')[0]
uploads, actions, errors = [], [], []
state = dict(title="", page=0, pages=0, busy=False, message="", font=0)

def serve(route):
    request = route.request
    path = urlparse(request.url).path
    if path == "/books":
        route.fulfill(body=html, content_type="text/html")
    elif path == "/reader.js":
        route.fulfill(body=script, content_type="text/javascript")
    elif path == "/api/books":
        route.fulfill(json={"books": [{"id": "1234abcd", "title": "Quiet pages"}] if uploads else []})
    elif path == "/api/reader":
        route.fulfill(json=state)
    elif path == "/api/books/upload":
        assert request.headers["x-paper-token"] == "123abc"
        uploads.append(request.post_data_buffer)
        route.fulfill(status=201, json={"title": "Quiet pages", "id": "1234abcd"})
    elif path == "/api/reader/action":
        assert request.headers["x-paper-token"] == "123abc"
        actions.append(request.post_data)
        route.fulfill(status=202, json={"queued": True})
    else:
        route.fulfill(status=404, body="Not found")

with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=args.chromium, headless=True, args=["--no-sandbox"])
    page = browser.new_page(viewport={"width": 390, "height": 844})
    page.route("http://paper.test/**", serve)
    page.on("pageerror", lambda e: errors.append(str(e)))
    page.goto("http://paper.test/books", wait_until="networkidle")
    page.locator("#file").set_input_files({"name": "Quiet pages.txt", "mimeType": "text/plain", "buffer": "“Café”\r\nby the sea.\r\n\r\nA new day.".encode()})
    page.wait_for_function("!document.getElementById('upload').disabled")
    assert page.locator("#preview").inner_text() == '"Cafe" by the sea.\n\nA new day.\n'
    page.locator("#upload").click()
    page.wait_for_function("document.getElementById('message').textContent.includes('Saved Quiet pages')")
    assert b'"Cafe" by the sea.\n\nA new day.\n' in uploads[0]
    page.get_by_role("button", name="Read Quiet pages", exact=True).click()
    page.wait_for_timeout(100)
    assert actions[-1] == "action=open&id=1234abcd"
    page.select_option("#font", "2")
    page.locator("#applyFont").click()
    page.wait_for_timeout(100)
    assert actions[-1] == "action=font&value=2"
    state.update(title="Quiet pages", page=2, pages=5, busy=True)
    page.wait_for_function("document.getElementById('jump').disabled")
    assert "refreshing" in page.locator("#state").inner_text()
    page.locator("#file").set_input_files({"name": "unsupported.txt", "mimeType": "text/plain", "buffer": "漢字".encode()})
    page.wait_for_function("document.getElementById('message').textContent.includes('unsupported characters')")
    assert page.locator("#upload").is_disabled()
    assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
    assert not errors, errors
    browser.close()
print("Reader browser: import preview, multipart upload, library, controls, busy state, rejection and mobile layout passed.")
