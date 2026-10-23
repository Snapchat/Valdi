#include "valdi_linux_window/SDLWindowHost.hpp"
#include "valdi_linux_window/Framebuffer.hpp"

#include "snap_drawing/cpp/Drawing/GraphicsContext/BitmapGraphicsContext.hpp"
#include "snap_drawing/cpp/Drawing/Surface/DrawableSurfaceCanvas.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Touches/TouchEvent.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace ValdiLinux {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto kFrameInterval = std::chrono::microseconds(16667);
constexpr size_t kMaxEventsPerPump = 256;
constexpr size_t kMaxTasksPerPump = 32;

Valdi::Error sdlError() { return Valdi::Error(SDL_GetError()); }

class QueueWakeup final : public Valdi::IQueueListener {
public:
    QueueWakeup(uint32_t eventType, Valdi::Shared<Valdi::IQueueListener> previous)
        : _eventType(eventType), _previous(std::move(previous)) {}
    void onQueueEmpty() override {
        if (_previous) {
            _previous->onQueueEmpty();
        }
    }
    void onQueueNonEmpty() override {
        // Called under TaskQueue's lock: never access the queue here.
        if (_previous) {
            _previous->onQueueNonEmpty();
        }
        SDL_Event event{};
        event.type = _eventType;
        SDL_PushEvent(&event); // Timed pumping also covers a full SDL queue.
    }

private:
    uint32_t _eventType;
    Valdi::Shared<Valdi::IQueueListener> _previous;
};
} // namespace

struct SDLWindowHost::Impl {
    Valdi::Ref<snap::drawing::LayerRoot> root;
    WindowClient& client;
    Valdi::TaskQueue* mainQueue;
    Valdi::Shared<Valdi::IQueueListener> previousQueueListener;
    SDL_Window* window = nullptr;
    WindowOptions options;
    WindowMetrics metrics;
    SystemTheme systemTheme = SystemTheme::Unknown;
    std::vector<uint8_t> pixels;
    snap::drawing::BitmapGraphicsContext graphics;
    bool videoInitialized = false;
    bool listenersAttached = false;
    bool closed = false;
    bool dirty = true;
    bool pointerDown = false;
    bool textInputRequested = false;
    bool focused = false;
    bool minimized = false;
    uint64_t presentedFrames = 0;
    snap::drawing::Point pointer;
    Clock::time_point nextFrame = Clock::now();

    Impl(Valdi::Ref<snap::drawing::LayerRoot> root, WindowClient& client, Valdi::TaskQueue* mainQueue)
        : root(std::move(root)), client(client), mainQueue(mainQueue) {}

    void dispatchPointer(snap::drawing::TouchEventType type, float x, float y, snap::drawing::Vector direction = {}) {
        pointer = {x, y};
        const bool active = type != snap::drawing::TouchEventTypeNone;
        snap::drawing::TouchEvent::PointerLocations locations;
        if (active) {
            locations.emplace_back(pointer);
        }
        // SDL timestamps have a different epoch. Use Valdi's clock consistently;
        // source latency is not measured by this prototype.
        root->dispatchTouchEvent(snap::drawing::TouchEvent(type, pointer, pointer, direction, active ? 1 : 0, 0,
                                                           std::move(locations), snap::drawing::TimePoint::now(),
                                                           snap::drawing::Duration(), nullptr));
    }

    void cancelPointer() {
        if (pointerDown) {
            pointerDown = false;
            dispatchPointer(snap::drawing::TouchEventTypeNone, pointer.x, pointer.y);
        }
    }

    void updateSystemTheme(bool initial = false) {
        SystemTheme theme = SystemTheme::Unknown;
        switch (SDL_GetSystemTheme()) {
            case SDL_SYSTEM_THEME_LIGHT: theme = SystemTheme::Light; break;
            case SDL_SYSTEM_THEME_DARK: theme = SystemTheme::Dark; break;
            default: break;
        }
        if (initial || systemTheme != theme) {
            systemTheme = theme;
            client.onSystemThemeChanged(theme);
        }
    }

    void event(const SDL_Event& event) {
        if (event.type == SDL_EVENT_QUIT) {
            closed = true;
            return;
        }
        const auto id = SDL_GetWindowID(window);
        switch (event.type) {
            case SDL_EVENT_SYSTEM_THEME_CHANGED:
                updateSystemTheme();
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event.window.windowID == id) {
                    closed = true;
                }
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                if (event.window.windowID != id) {
                    break;
                }
                focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
                if (!focused) {
                    cancelPointer();
                    SDL_StopTextInput(window);
                    client.onTextEditing("", -1, -1);
                } else if (textInputRequested) {
                    SDL_StartTextInput(window);
                }
                client.onFocusChanged(focused);
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                if (event.window.windowID == id) {
                    minimized = true;
                    cancelPointer();
                }
                break;
            case SDL_EVENT_WINDOW_RESTORED:
                if (event.window.windowID == id) {
                    minimized = false;
                }
                [[fallthrough]];
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            case SDL_EVENT_WINDOW_EXPOSED:
                if (event.window.windowID == id) {
                    dirty = true;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (event.key.windowID == id && focused) {
                    client.onKey({event.key.key, static_cast<uint32_t>(event.key.scancode), event.key.mod,
                                  event.key.down, event.key.repeat});
                }
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (event.text.windowID == id && focused && textInputRequested) {
                    client.onTextEditing("", -1, -1);
                    client.onTextInput(event.text.text ? event.text.text : "");
                }
                break;
            case SDL_EVENT_TEXT_EDITING:
                if (event.edit.windowID == id && focused && textInputRequested) {
                    client.onTextEditing(event.edit.text ? event.edit.text : "", event.edit.start, event.edit.length);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.windowID != id || event.button.button != SDL_BUTTON_LEFT || !focused) {
                    break;
                }
                if (event.button.down) {
                    pointerDown = true;
                    dispatchPointer(snap::drawing::TouchEventTypeDown, event.button.x, event.button.y);
                } else if (pointerDown) {
                    pointerDown = false;
                    dispatchPointer(snap::drawing::TouchEventTypeUp, event.button.x, event.button.y);
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.windowID == id && pointerDown) {
                    dispatchPointer(snap::drawing::TouchEventTypeMoved, event.motion.x, event.motion.y);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (event.wheel.windowID == id && focused) {
                    const float sign = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? 1 : -1;
                    // SDL wheel values are scroll steps, not pixels. A policy for
                    // step-to-content distance belongs in the application adapter.
                    dispatchPointer(snap::drawing::TouchEventTypeWheel, event.wheel.mouse_x, event.wheel.mouse_y,
                                    {sign * event.wheel.x, sign * event.wheel.y});
                }
                break;
            default:
                break;
        }
    }

    Valdi::Result<Valdi::Void> present() {
        // Reacquire every time: SDL invalidates this surface on resize.
        SDL_Surface* surface = SDL_GetWindowSurface(window);
        if (!surface) {
            return sdlError();
        }
        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSize(window, &width, &height)) {
            return sdlError();
        }
        if (width <= 0 || height <= 0 || surface->w <= 0 || surface->h <= 0) {
            return Valdi::Void();
        }
        const auto bytes = framebufferBytes(surface->w, surface->h, options.maxFramebufferBytes);
        if (!bytes) {
            return Valdi::Error("Window framebuffer exceeds the configured memory limit");
        }
        const float density = static_cast<float>(surface->w) / width;
        const float densityY = static_cast<float>(surface->h) / height;
        if (std::abs(density - densityY) > 0.02f) {
            return Valdi::Error("Non-uniform window pixel density is not supported by this prototype");
        }
        const float displayScale = SDL_GetWindowDisplayScale(window);
        if (metrics.width != width || metrics.height != height || metrics.pixelWidth != surface->w ||
            metrics.pixelHeight != surface->h || metrics.displayScale != displayScale) {
            metrics = {width, height, surface->w, surface->h, density, displayScale};
            root->getResources()->setDisplayScale(density);
            root->setSize({static_cast<float>(width), static_cast<float>(height)}, density);
            client.onResize(metrics);
        }
        pixels.resize(*bytes);
        const int rowBytes = surface->w * 4;
        const Valdi::BitmapInfo bitmap(surface->w, surface->h, Valdi::ColorTypeRGBA8888, Valdi::AlphaTypePremul,
                                       rowBytes);
        // Keep this drawable local: no Skia object can retain a pointer across a
        // subsequent vector resize or window-surface invalidation.
        auto drawable = graphics.createBitmapSurface(bitmap, pixels.data());
        if (drawable == nullptr) {
            return Valdi::Error("Unable to create the SnapDrawing bitmap surface");
        }
        auto canvas = drawable->prepareCanvas();
        if (!canvas) {
            return canvas.error();
        }
        root->drawInCanvas(canvas.value());
        drawable->flush();
        const bool mustLock = SDL_MUSTLOCK(surface);
        if (mustLock && !SDL_LockSurface(surface)) {
            return sdlError();
        }
        const bool converted = SDL_ConvertPixels(surface->w, surface->h, SDL_PIXELFORMAT_RGBA32, pixels.data(),
                                                 rowBytes, surface->format, surface->pixels, surface->pitch);
        if (mustLock) {
            SDL_UnlockSurface(surface);
        }
        if (!converted) {
            return sdlError();
        }
        if (!SDL_UpdateWindowSurface(window)) {
            return sdlError();
        }
        ++presentedFrames;
        dirty = false;
        return Valdi::Void();
    }
};

SDLWindowHost::SDLWindowHost(const Valdi::Ref<snap::drawing::LayerRoot>& root, WindowClient& client,
                             Valdi::TaskQueue* mainQueue)
    : _impl(std::make_unique<Impl>(root, client, mainQueue)) {}

SDLWindowHost::~SDLWindowHost() {
    auto& p = *_impl;
    if (p.listenersAttached) {
        if (p.mainQueue) {
            p.mainQueue->setListener(p.previousQueueListener);
        }
        p.root->setListener(nullptr);
        p.cancelPointer();
    }
    if (p.window) {
        SDL_StopTextInput(p.window);
        SDL_DestroyWindow(p.window);
    }
    if (p.videoInitialized) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

Valdi::Result<Valdi::Void> SDLWindowHost::open(const WindowOptions& options) {
    auto& p = *_impl;
    if (p.window || p.videoInitialized) {
        return Valdi::Error("Window host can only be opened once");
    }
    if (p.root == nullptr || p.root->getListener()) {
        return Valdi::Error("Window host requires a root and an unused root listener slot");
    }
    if (!framebufferBytes(options.width, options.height, options.maxFramebufferBytes)) {
        return Valdi::Error("Invalid initial window dimensions or framebuffer limit");
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return sdlError();
    }
    p.videoInitialized = true;
    p.options = options;
    p.window = SDL_CreateWindow(options.title.c_str(), options.width, options.height,
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!p.window) {
        return sdlError();
    }
    const uint32_t wakeEvent = SDL_RegisterEvents(1);
    if (!wakeEvent) {
        return sdlError();
    }
    p.focused = (SDL_GetWindowFlags(p.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
    p.root->setListener(this);
    if (p.mainQueue) {
        p.previousQueueListener = p.mainQueue->getListener();
        p.mainQueue->setListener(std::make_shared<QueueWakeup>(wakeEvent, p.previousQueueListener));
    }
    p.listenersAttached = true;
    p.updateSystemTheme(true);
    return Valdi::Void();
}

Valdi::Result<bool> SDLWindowHost::pump(int maxWaitMs) {
    auto& p = *_impl;
    if (!p.window) {
        return Valdi::Error("Window host is not open");
    }
    if (p.closed) {
        return false;
    }
    int wait = std::clamp(maxWaitMs, 0, 1000);
    // TaskQueue does not expose its next deadline. Keep delayed tasks responsive
    // even when the queue-empty listener cannot signal another transition.
    if (p.mainQueue) {
        wait = std::min(wait, 16);
    }
    if (p.dirty || p.root->needsProcessFrame()) {
        const auto untilFrame = std::chrono::ceil<std::chrono::milliseconds>(p.nextFrame - Clock::now()).count();
        wait = std::min(wait, static_cast<int>(std::max<int64_t>(0, untilFrame)));
    }
    SDL_Event event{};
    if (SDL_WaitEventTimeout(&event, wait)) {
        p.event(event);
        for (size_t i = 1; i < kMaxEventsPerPump && SDL_PollEvent(&event); ++i) {
            p.event(event);
        }
    }
    if (p.closed) {
        return false;
    }
    if (p.mainQueue) {
        const auto deadline = Clock::now() + std::chrono::milliseconds(2);
        for (size_t i = 0; i < kMaxTasksPerPump && Clock::now() < deadline; ++i) {
            if (!p.mainQueue->runNextTask(Clock::now())) {
                break;
            }
        }
    }
    if (Clock::now() >= p.nextFrame) {
        p.nextFrame = Clock::now() + kFrameInterval;
        if (p.pointerDown) {
            p.root->refreshTouches(snap::drawing::TimePoint::now());
        }
        if (p.root->needsProcessFrame()) {
            p.root->processFrame(snap::drawing::TimePoint::now());
        }
        if (p.dirty && !p.minimized) {
            auto result = p.present();
            if (!result) {
                return result.error();
            }
        }
    }
    return true;
}

void SDLWindowHost::requestClose() { _impl->closed = true; }

Valdi::Result<Valdi::Void> SDLWindowHost::setTextInput(bool enabled, int x, int y, int width, int height,
                                                       int cursorOffset) {
    auto& p = *_impl;
    if (!p.window) {
        return Valdi::Error("Window host is not open");
    }
    if (width < 0 || height < 0) {
        return Valdi::Error("Invalid text input rectangle");
    }
    SDL_Rect rect{x, y, width, height};
    if (!SDL_SetTextInputArea(p.window, &rect, cursorOffset)) {
        return sdlError();
    }
    p.textInputRequested = enabled;
    if (enabled && p.focused) {
        if (!SDL_StartTextInput(p.window)) {
            return sdlError();
        }
    } else {
        if (!SDL_StopTextInput(p.window)) {
            return sdlError();
        }
        p.client.onTextEditing("", -1, -1);
    }
    return Valdi::Void();
}

Valdi::Result<std::string> SDLWindowHost::getClipboardText() const {
    char* text = SDL_GetClipboardText();
    if (!text) {
        return sdlError();
    }
    std::string copy(text);
    SDL_free(text);
    return copy;
}

Valdi::Result<Valdi::Void> SDLWindowHost::setClipboardText(const std::string& text) {
    if (!SDL_SetClipboardText(text.c_str())) {
        return sdlError();
    }
    return Valdi::Void();
}

SystemTheme SDLWindowHost::getSystemTheme() const { return _impl->systemTheme; }

const WindowMetrics& SDLWindowHost::getMetrics() const { return _impl->metrics; }
uint32_t SDLWindowHost::getWindowId() const { return SDL_GetWindowID(_impl->window); }
uint64_t SDLWindowHost::getPresentedFrames() const { return _impl->presentedFrames; }
void SDLWindowHost::onNeedsProcessFrame(snap::drawing::LayerRoot&) {}
void SDLWindowHost::onDidDraw(snap::drawing::LayerRoot&, const Valdi::Ref<snap::drawing::DisplayList>&,
                              const snap::drawing::CompositorPlaneList*) {
    _impl->dirty = true;
}

} // namespace ValdiLinux
