#pragma once
#include <cstdint>
namespace RefreshPalette {
// M5Canvas::readPixel returns RGB565, not RGB888. The chart uses only these
// exact six colours and monochrome bitmap fonts; any other value becomes black.
inline uint8_t native(uint16_t rgb565){
    switch(rgb565){case 0xffff:return 1;case 0xf800:return 3;case 0xffe0:return 2;case 0x001f:return 5;case 0x07e0:return 6;default:return 0;}
}
}
