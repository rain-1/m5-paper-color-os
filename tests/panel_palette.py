"""Exercise the pinned driver's actual no-dither conversion on the host."""
from pathlib import Path
import subprocess
import tempfile

driver = Path('.pio/libdeps/papercolor/M5GFX/src/lgfx/v1/panel/Panel_ED2208.cpp').read_text()
# Compile the unmodified palette and conversion functions, without ESP32 I/O.
conversion = driver.split('// EPD color indices', 1)[1].split('// --- Dither: Bayer RGB', 1)[0]
conversion = '// EPD color indices' + conversion
transfer = driver.split('void Panel_ED2208::_exec_transfer(void)', 1)[1].split('void Panel_ED2208::setSleep', 1)[0]
assert 'case epd_mode_t::epd_fastest: dither_fn = _dither_row_none;' in transfer
picture_branch = Path('src/main.cpp').read_text().split('if (screen.picture[0])', 1)[1].split('canvas.fillSprite', 1)[0]
actions = ['setEpdMode(epd_mode_t::epd_fastest)', 'canvas.pushSprite',
           'M5.Display.waitDisplay()', 'setEpdMode(epd_mode_t::epd_quality)']
assert [picture_branch.index(a) for a in actions] == sorted(picture_branch.index(a) for a in actions)
source = r'''
#include <cstdint>
#include <cstddef>
#include <cassert>
namespace lgfx { struct bgr888_t { uint8_t b,g,r; }; }
''' + conversion + r'''
int main() {
    const lgfx::bgr888_t colours[]={{0,0,0},{255,255,255},{0,255,255},
                                   {0,0,255},{255,0,0},{0,255,0}};
    const uint8_t native[]={0,1,2,3,5,6};
    lgfx::bgr888_t row[400]; uint8_t out[200];
    // Every ordered pair, at every position: neither colour nor texture changes.
    for (int a=0;a<6;++a) for(int b=0;b<6;++b) {
        for(int x=0;x<400;++x) row[x]=colours[(x&1)?b:a];
        for(int y=0;y<600;++y) {
            _dither_row_none(row,out,400,y,140);
            for(auto packed:out) assert(packed==((native[a]<<4)|native[b]));
        }
    }
}
'''
with tempfile.TemporaryDirectory(prefix='paper-palette-') as directory:
    executable = str(Path(directory) / 'test')
    subprocess.run(['g++', '-std=c++11', '-O2', '-x', 'c++', '-', '-o', executable],
                   input=source, text=True, check=True)
    subprocess.run([executable], check=True)
print('Panel palette: all 36 colour pairs preserved across 400x600; picture mode ordering passed.')
