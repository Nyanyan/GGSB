#pragma once
#include <algorithm>
#include <cmath>

namespace ggsb {
// Shared list mechanics; fractional wheel input is retained between frames.
struct ListScroll {
    double wheel_remainder = 0;
    static int maximum(int count, int visible) { return std::max(0, count - visible); }
    void wheel(int& offset, int count, int visible, double delta, double speed) {
        const int maximum_offset = maximum(count, visible);
        offset = std::clamp(offset, 0, maximum_offset);
        wheel_remainder += delta * speed;
        const int steps = int(wheel_remainder);
        wheel_remainder -= steps;
        offset = std::clamp(offset + steps, 0, maximum_offset);
        if ((offset == 0 && wheel_remainder < 0) || (offset == maximum_offset && wheel_remainder > 0)) wheel_remainder = 0;
    }
    static int drag(int count, int visible, double fraction) {
        return int(std::lround(std::clamp(fraction, 0.0, 1.0) * maximum(count, visible)));
    }
};
}
