#include "valdi_linux_window/SDLWindowHost.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Touches/SingleTapGestureRecognizer.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace sd = snap::drawing;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::cerr << "Failed: " #condition << " at " << __LINE__ << '\n';                                          \
            std::exit(1);                                                                                              \
        }                                                                                                              \
    } while (false)

struct Client final : ValdiLinux::WindowClient {
    std::string committed;
    std::string preedit;
    int32_t start = -1;
    int32_t length = -1;
    int keys = 0;
    bool focused = false;
    void onTextInput(const std::string& text) override { committed += text; }
    void onTextEditing(const std::string& text, int32_t s, int32_t len) override {
        preedit = text;
        start = s;
        length = len;
    }
    void onKey(const ValdiLinux::KeyEvent&) override { ++keys; }
    void onFocusChanged(bool value) override { focused = value; }
};

struct QueueObserver final : Valdi::IQueueListener {
    std::atomic<int> notifications{0};
    void onQueueEmpty() override { ++notifications; }
    void onQueueNonEmpty() override { ++notifications; }
};

void windowEvent(ValdiLinux::SDLWindowHost& host, uint32_t type) {
    SDL_Event event{};
    event.type = type;
    event.window.windowID = host.getWindowId();
    CHECK(SDL_PushEvent(&event));
}

void pump(ValdiLinux::SDLWindowHost& host, int count = 4) {
    for (int i = 0; i < count; ++i) {
        auto result = host.pump(20);
        if (!result) {
            std::cerr << result.error() << '\n';
        }
        CHECK(result && result.value());
    }
}

int main(int argc, char** argv) {
    auto fonts = Valdi::makeShared<sd::FontManager>(Valdi::ConsoleLogger::getLogger());
    auto resources = Valdi::makeShared<sd::Resources>(fonts, 1, Valdi::ConsoleLogger::getLogger());
    for (int cycle = 0; cycle < 3; ++cycle) {
        auto root = Valdi::makeShared<sd::LayerRoot>(resources);
        auto content = sd::makeLayer<sd::Layer>(resources);
        content->setBackgroundColor(sd::Color::make(0xff123456));
        root->setContentLayer(content, sd::ContentLayerSizingModeMatchSize);
        Valdi::TaskQueue queue;
        auto observer = std::make_shared<QueueObserver>();
        if (cycle == 1) {
            queue.setListener(observer);
        }
        Client client;
        int taps = 0;
        auto tap = Valdi::makeShared<sd::SingleTapGestureRecognizer>(resources->getGesturesConfiguration());
        tap->setListener([&](const auto&, auto state, const auto&) {
            if (state == sd::GestureRecognizerStateEnded) {
                ++taps;
            }
        });
        content->addGestureRecognizer(tap);
        {
            ValdiLinux::SDLWindowHost host(root, client, &queue);
            ValdiLinux::WindowOptions options;
            options.width = 320;
            options.height = 240;
            auto opened = host.open(options);
            if (!opened) {
                std::cerr << opened.error() << '\n';
            }
            CHECK(opened);
            pump(host);
            CHECK(host.getPresentedFrames() > 0);
            const auto initialMetrics = host.getMetrics();
            CHECK(initialMetrics.width == 320 && initialMetrics.height == 240);
            CHECK(initialMetrics.pixelWidth >= 320 && initialMetrics.pixelHeight >= 240);
            auto* window = SDL_GetWindowFromID(host.getWindowId());
            auto* surface = SDL_GetWindowSurface(window);
            uint8_t r = 0, g = 0, b = 0, a = 0;
            CHECK(SDL_ReadSurfacePixel(surface, 10, 10, &r, &g, &b, &a));
            CHECK(r == 0x12 && g == 0x34 && b == 0x56);

            // Event injection verifies the host boundary, not an OS IME server.
            windowEvent(host, SDL_EVENT_WINDOW_FOCUS_GAINED);
            pump(host);
            CHECK(host.setTextInput(true, 12, 24, 200, 32, 15));
            SDL_Rect rect{};
            int cursor = -1;
            CHECK(SDL_GetTextInputArea(window, &rect, &cursor));
            CHECK(rect.x == 12 && rect.y == 24 && cursor == 15);
            CHECK(SDL_TextInputActive(window));
            SDL_Event event{};
            event.type = SDL_EVENT_TEXT_EDITING;
            event.edit.windowID = host.getWindowId();
            event.edit.text = "éあ🙂";
            event.edit.start = 1;
            event.edit.length = 2;
            CHECK(SDL_PushEvent(&event));
            pump(host);
            CHECK(client.preedit == "éあ🙂" && client.start == 1 && client.length == 2);
            CHECK(client.committed.empty());
            event = {};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.windowID = host.getWindowId();
            event.key.key = SDLK_A;
            event.key.down = true;
            CHECK(SDL_PushEvent(&event));
            pump(host);
            CHECK(client.keys == 1 && client.committed.empty());
            event = {};
            event.type = SDL_EVENT_TEXT_INPUT;
            event.text.windowID = host.getWindowId();
            event.text.text = "éあ🙂";
            CHECK(SDL_PushEvent(&event));
            pump(host);
            CHECK(client.committed == "éあ🙂" && client.preedit.empty());
            event.text.windowID = host.getWindowId() + 100;
            CHECK(SDL_PushEvent(&event));
            pump(host);
            CHECK(client.committed == "éあ🙂");
            CHECK(host.setClipboardText("clipboard é🙂"));
            auto clipboard = host.getClipboardText();
            CHECK(clipboard && clipboard.value() == "clipboard é🙂");

            for (bool down : {true, false}) {
                event = {};
                event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                event.button.windowID = host.getWindowId();
                event.button.button = SDL_BUTTON_LEFT;
                event.button.down = down;
                event.button.x = 30;
                event.button.y = 40;
                CHECK(SDL_PushEvent(&event));
                pump(host);
            }
            CHECK(taps == 1);
            event = {};
            event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            event.button.windowID = host.getWindowId();
            event.button.button = SDL_BUTTON_LEFT;
            event.button.down = true;
            event.button.x = 30;
            event.button.y = 40;
            CHECK(SDL_PushEvent(&event));
            pump(host);
            windowEvent(host, SDL_EVENT_WINDOW_FOCUS_LOST);
            pump(host);
            CHECK(!client.focused && client.preedit.empty() && !SDL_TextInputActive(window));
            event.type = SDL_EVENT_MOUSE_BUTTON_UP;
            event.button.down = false;
            CHECK(SDL_PushEvent(&event));
            pump(host);
            CHECK(taps == 1);
            event = {};
            event.type = SDL_EVENT_TEXT_INPUT;
            event.text.windowID = host.getWindowId();
            event.text.text = "ignored";
            CHECK(SDL_PushEvent(&event));
            pump(host);
            CHECK(client.committed == "éあ🙂");

            CHECK(SDL_SetWindowSize(window, 480, 300));
            const auto resizeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while ((host.getMetrics().width != 480 || host.getMetrics().height != 300) &&
                   std::chrono::steady_clock::now() < resizeDeadline) {
                pump(host, 1);
            }
            CHECK(host.getMetrics().width == 480 && host.getMetrics().height == 300);
            if (cycle == 0 && argc > 1) {
                CHECK(SDL_SaveBMP(SDL_GetWindowSurface(window), argv[1]));
            }
            int completed = 0;
            std::thread worker([&] { queue.enqueue([&] { ++completed; }); });
            worker.join();
            pump(host);
            CHECK(completed == 1);
            queue.enqueue([&] { ++completed; }, std::chrono::milliseconds(30));
            pump(host, 8);
            CHECK(completed == 2);
            for (int i = 0; i < 100; ++i) {
                queue.enqueue([&] { ++completed; });
            }
            pump(host, 1);
            CHECK(completed > 2 && completed <= 34);
            pump(host, 8);
            CHECK(completed == 102);

            const auto beforeUpdate = host.getPresentedFrames();
            queue.enqueue([&] { content->setBackgroundColor(sd::Color::make(0xff654321)); });
            pump(host, 8);
            CHECK(host.getPresentedFrames() > beforeUpdate);
            CHECK(SDL_ReadSurfacePixel(SDL_GetWindowSurface(window), 10, 10, &r, &g, &b, &a));
            CHECK(r == 0x65 && g == 0x43 && b == 0x21);

            // Static content must not be continually rasterized during idle.
            pump(host);
            const auto beforeIdle = host.getPresentedFrames();
            pump(host, 8);
            CHECK(host.getPresentedFrames() == beforeIdle);
            windowEvent(host, SDL_EVENT_WINDOW_CLOSE_REQUESTED);
            auto closed = host.pump(20);
            CHECK(closed && !closed.value());
            std::cout << "cycle=" << cycle << " density=" << initialMetrics.pixelDensity
                      << " displayScale=" << initialMetrics.displayScale << " pixels=" << initialMetrics.pixelWidth
                      << 'x' << initialMetrics.pixelHeight << " frames=" << host.getPresentedFrames() << '\n';
        }
        CHECK(!root->getListener());
        if (cycle == 1) {
            CHECK(queue.getListener() == observer && observer->notifications > 0);
        } else {
            CHECK(!queue.getListener());
        }
        root->destroy();
    }
    std::cout << "PASS: raster pixels, resize, text boundary, clipboard, tap, queue, idle, teardown\n";
    return 0;
}
