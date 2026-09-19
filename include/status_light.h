#pragma once
#include <cstdint>
// All calls owned by main task; menu selection overrides transient cues.
namespace StatusLight {
bool begin();
bool ready();
void flash(uint32_t colour,uint32_t duration,int index=-1);
void tick(int menuSelection);
}
