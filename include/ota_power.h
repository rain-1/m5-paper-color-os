#pragma once

namespace OtaPower {
constexpr int minimumPercent = 30;
constexpr int minimumBatteryMv = 3600;
// Conservative development thresholds, not a battery runtime guarantee.
inline bool externalPower(int inputMv) { return inputMv >= 4600 && inputMv <= 5500; }
inline bool allowed(int inputMv, int batteryMv, int percent) {
    if (externalPower(inputMv)) return true;
    return batteryMv >= minimumBatteryMv && batteryMv <= 4350 &&
           percent >= minimumPercent && percent <= 100;
}
}
