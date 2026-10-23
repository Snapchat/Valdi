#include "valdi_linux_window/Framebuffer.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::cerr << "Failed: " #condition << '\n';                                                                \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)

int main() {
    using ValdiLinux::framebufferBytes;
    CHECK(framebufferBytes(800, 600, 1920000) == 1920000);
    CHECK(!framebufferBytes(800, 600, 1919999));
    CHECK(!framebufferBytes(0, 600, 1920000));
    CHECK(!framebufferBytes(-1, 600, 1920000));
    CHECK(!framebufferBytes(800, -1, 1920000));
    CHECK(!framebufferBytes(std::numeric_limits<int>::max(), 1, std::numeric_limits<size_t>::max()));
    CHECK(!framebufferBytes(8192, 8192, 128 * 1024 * 1024));
    return 0;
}
