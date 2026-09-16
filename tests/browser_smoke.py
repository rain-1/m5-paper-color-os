"""Optional: pip install playwright. Runs against the live device without saving images."""
import argparse
import base64
from playwright.sync_api import sync_playwright

parser = argparse.ArgumentParser()
parser.add_argument("base")
parser.add_argument("--chromium", required=True)
args = parser.parse_args()
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path=args.chromium, headless=True, args=["--no-sandbox"])
    page = browser.new_page(viewport={"width": 390, "height": 844})
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))
    page.goto(args.base + "/gallery", wait_until="networkidle")
    assert "SD card ready" in page.locator("#card").inner_text()
    png = page.evaluate("""() => {
        const c=document.createElement('canvas');c.width=800;c.height=200;
        const x=c.getContext('2d');x.fillStyle='red';x.fillRect(0,0,800,200);
        return c.toDataURL('image/png').split(',')[1];
    }""")
    page.locator("#file").set_input_files({"name": "wide.png", "mimeType": "image/png", "buffer": base64.b64decode(png)})
    page.wait_for_function("!document.getElementById('upload').disabled")
    colors = page.evaluate("""() => {
        const x=document.getElementById('preview').getContext('2d');
        return [Array.from(x.getImageData(200,20,1,1).data),Array.from(x.getImageData(200,300,1,1).data)];
    }""")
    assert colors == [[255,255,255,255],[255,0,0,255]], colors
    page.select_option("#dither", "no")
    assert page.locator("#upload").is_enabled()
    assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
    page.goto(args.base + "/device", wait_until="networkidle")
    page.wait_for_function("document.getElementById('status').textContent.includes('Battery:')")
    assert "Button presses:" in page.locator("#status").inner_text()
    assert "Update power: OK" in page.locator("#power").inner_text()
    assert page.locator("#update").is_disabled()  # Reboot closes the physical unlock window.
    assert page.locator("#sounds").is_checked()
    assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
    assert not errors, errors
    print("Browser: local scripts, PNG decode, aspect fit, palette preview, controls, diagnostics and mobile layout passed.")
    browser.close()
