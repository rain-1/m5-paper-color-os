#pragma once

namespace Feedback {
enum class Cue { Connected, Saved, Error, Busy, Unlocked, Updated, ToneTest };
void begin();
void tick();
// Thread-safe request; hardware playback is owned by tick() on the main task.
void play(Cue cue);
bool enabled();
void setEnabled(bool enabled);
}
