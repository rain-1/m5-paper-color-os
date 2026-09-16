#pragma once

// Entirely local: captive browsers need no internet, fonts, or JavaScript.
constexpr char PORTAL_HTML[] = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Paper OS · Wi-Fi</title><style>
*{box-sizing:border-box}body{margin:0;background:#f4f1e8;color:#19231e;font:17px system-ui,sans-serif}
main{max-width:460px;margin:8vh auto;padding:28px}header{border-top:8px solid #287851;padding-top:24px}
h1{font-size:42px;letter-spacing:-2px;margin:16px 0}p{line-height:1.5}label{display:block;margin-top:24px}
input,button{width:100%;padding:15px;border:1px solid #89958c;border-radius:8px;font:inherit;margin-top:8px}
button{background:#195d3b;color:white;border:0;margin-top:28px;cursor:pointer}small{color:#526158}
</style><main><header><small>PAPER OS / FIRST EDITION</small><h1>A little more<br>connected.</h1></header>
<p>Give your PaperColor a 2.4 GHz Wi-Fi network. It will remember it next time.</p>
<form method="post" action="/connect"><input type="hidden" name="token" value="{{TOKEN}}">
<label for="ssid">Network name</label><input id="ssid" name="ssid" maxlength="32" required autocomplete="off">
<label for="password">Wi-Fi password</label><input id="password" name="password" type="password" maxlength="63" autocomplete="new-password">
<small>Leave the password empty for an open network. Hidden networks work too.</small>
<button>Connect PaperColor</button></form><p><small>Your credentials stay on this device.</small></p></main></html>)HTML";
