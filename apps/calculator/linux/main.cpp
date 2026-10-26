#include "valdi_linux_window/ValdiWindowApplication.hpp"
#include "valdi_core/cpp/Utils/ValueFunctionWithCallable.hpp"

#include <SDL3/SDL.h>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>

#ifdef VALDI_LINUX_THEME_TEST
void setTestSystemTheme(SDL_SystemTheme theme);
#endif

namespace {
struct Frame {
    std::string parent;
    double x, y, width, height;
};

// This demo bridge owns no window/runtime. JS callbacks capture shared state,
// never a native client pointer, and native->JS calls happen outside its lock.
struct CalculatorBridge {
    std::mutex mutex;
    Valdi::Ref<Valdi::ValueFunction> input;
    std::string display;
    std::string expression;
    std::map<std::string, Frame> frames;
    bool active = true;
    int disconnects = 0;

    bool send(const std::string& key) {
        Valdi::Ref<Valdi::ValueFunction> callback;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (active) {
                callback = input;
            }
        }
        if (callback == nullptr) {
            return false;
        }
        auto result =
            callback->call(Valdi::ValueFunctionFlagsNeverCallSync, {Valdi::Value(Valdi::StringBox::fromString(key))});
        if (!result) {
            std::cerr << result.error() << '\n';
        }
        return result.success();
    }

    std::string readDisplay() {
        std::lock_guard<std::mutex> lock(mutex);
        return display;
    }

    std::optional<SDL_FPoint> keyCenter(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex);
        auto found = frames.find(key);
        if (found == frames.end() || found->second.width <= 0 || found->second.height <= 0) {
            return std::nullopt;
        }
        const auto& frame = found->second;
        double x = frame.x + frame.width / 2;
        double y = frame.y + frame.height / 2;
        auto parent = frame.parent;
        for (int depth = 0; !parent.empty() && depth < 8; ++depth) {
            auto ancestor = frames.find(parent);
            if (ancestor == frames.end()) {
                return std::nullopt;
            }
            x += ancestor->second.x;
            y += ancestor->second.y;
            parent = ancestor->second.parent;
        }
        if (!parent.empty()) {
            return std::nullopt;
        }
        return SDL_FPoint{static_cast<float>(x), static_cast<float>(y)};
    }

    Valdi::Value context(const std::shared_ptr<CalculatorBridge>& self) {
        Valdi::Value value;
        value.setMapValue("fontFamily", Valdi::Value(Valdi::StringBox::fromString("Window")));
        value.setMapValue("connect",
                          Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
                              [self](const Valdi::ValueFunctionCallContext& call) {
                                  std::lock_guard<std::mutex> lock(self->mutex);
                                  if (call.getParametersSize() && call.getParameter(0).isFunction() && self->active) {
                                      self->input = call.getParameter(0).getFunctionRef();
                                  } else {
                                      self->input = nullptr;
                                      ++self->disconnects;
                                  }
                                  return Valdi::Value();
                              })));
        value.setMapValue("reportState",
                          Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
                              [self](const Valdi::ValueFunctionCallContext& call) {
                                  const auto display = std::string(call.getParameterAsString(0).toStringView());
                                  const auto expression = std::string(call.getParameterAsString(1).toStringView());
                                  std::lock_guard<std::mutex> lock(self->mutex);
                                  self->display = display;
                                  self->expression = expression;
                                  return Valdi::Value();
                              })));
        value.setMapValue("reportLayout",
                          Valdi::Value(Valdi::makeShared<Valdi::ValueFunctionWithCallable>(
                              [self](const Valdi::ValueFunctionCallContext& call) {
                                  const auto id = std::string(call.getParameterAsString(0).toStringView());
                                  Frame frame{std::string(call.getParameterAsString(1).toStringView()),
                                              call.getParameterAsDouble(2), call.getParameterAsDouble(3),
                                              call.getParameterAsDouble(4), call.getParameterAsDouble(5)};
                                  std::lock_guard<std::mutex> lock(self->mutex);
                                  if (self->frames.size() < 32 || self->frames.count(id)) {
                                      self->frames[id] = std::move(frame);
                                  }
                                  return Valdi::Value();
                              })));
        return value;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex);
        active = false;
        input = nullptr;
    }
};

class CalculatorClient final : public ValdiLinux::WindowClient {
public:
    explicit CalculatorClient(std::shared_ptr<CalculatorBridge> bridge) : _bridge(std::move(bridge)) {}
    void onKey(const ValdiLinux::KeyEvent& event) override {
        if (!event.down || (event.modifiers & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))) {
            return;
        }
        std::string key;
        if (event.key >= SDLK_0 && event.key <= SDLK_9) {
            key = char('0' + event.key - SDLK_0);
        } else if (event.key >= SDLK_KP_1 && event.key <= SDLK_KP_9) {
            key = char('1' + event.key - SDLK_KP_1);
        } else {
            switch (event.key) {
                case SDLK_KP_0:
                    key = "0";
                    break;
                case SDLK_PLUS:
                case SDLK_KP_PLUS:
                    key = "+";
                    break;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:
                    key = "-";
                    break;
                case SDLK_ASTERISK:
                case SDLK_KP_MULTIPLY:
                    key = "*";
                    break;
                case SDLK_SLASH:
                case SDLK_KP_DIVIDE:
                    key = "/";
                    break;
                case SDLK_PERIOD:
                case SDLK_KP_PERIOD:
                    key = ".";
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_EQUALS:
                    key = "=";
                    break;
                case SDLK_ESCAPE:
                    key = "clear";
                    break;
                case SDLK_BACKSPACE:
                case SDLK_DELETE:
                    key = "Backspace";
                    break;
                default:
                    break;
            }
        }
        if (!key.empty()) {
            _bridge->send(key);
        }
    }

private:
    std::shared_ptr<CalculatorBridge> _bridge;
};

bool pump(ValdiLinux::SDLWindowHost& host) {
    auto result = host.pump(16);
    if (!result) {
        std::cerr << result.error() << '\n';
    }
    return result && result.value();
}

template <typename Predicate> bool waitFor(ValdiLinux::SDLWindowHost& host, Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() > deadline || !pump(host)) {
            return false;
        }
    }
    return true;
}

bool click(ValdiLinux::SDLWindowHost& host, CalculatorBridge& bridge, const std::string& key, const char* expected) {
    const auto center = bridge.keyCenter(key);
    if (!center) {
        std::cerr << "No laid-out key " << key << '\n';
        return false;
    }
    for (bool down : {true, false}) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = host.getWindowId();
        event.button.button = SDL_BUTTON_LEFT;
        event.button.down = down;
        event.button.x = center->x;
        event.button.y = center->y;
        if (!SDL_PushEvent(&event) || !pump(host)) {
            return false;
        }
    }
    const bool matched = waitFor(host, [&] { return bridge.readDisplay() == expected; });
    if (!matched) {
        std::cerr << "Tap " << key << ": expected " << expected << ", got " << bridge.readDisplay() << " at "
                  << center->x << ',' << center->y << '\n';
    }
    return matched;
}

bool keyboard(ValdiLinux::SDLWindowHost& host, CalculatorBridge& bridge, SDL_Keycode key, const char* expected) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = host.getWindowId();
    event.key.key = key;
    event.key.down = true;
    if (!SDL_PushEvent(&event) || !pump(host)) {
        return false;
    }
    return waitFor(host, [&] { return bridge.readDisplay() == expected; });
}

bool selfTest(ValdiLinux::SDLWindowHost& host, CalculatorBridge& bridge, const char* screenshot) {
    if (!waitFor(host, [&] {
            return bridge.readDisplay() == "0" && bridge.keyCenter("=").has_value() && host.getPresentedFrames() >= 2;
        })) {
        return false;
    }
#ifdef VALDI_LINUX_THEME_TEST
    auto pixelIs = [&](bool dark) {
        uint8_t r = 0, g = 0, b = 0, a = 0;
        auto* surface = SDL_GetWindowSurface(SDL_GetWindowFromID(host.getWindowId()));
        return surface && SDL_ReadSurfacePixel(surface, 2, 2, &r, &g, &b, &a) &&
               r == (dark ? 0x10 : 0xf4) && g == (dark ? 0x15 : 0xf6) && b == (dark ? 0x1f : 0xfa);
    };
    if (!waitFor(host, [&] { return pixelIs(host.getSystemTheme() == ValdiLinux::SystemTheme::Dark); })) return false;
    for (auto theme : {SDL_SYSTEM_THEME_DARK, SDL_SYSTEM_THEME_LIGHT, SDL_SYSTEM_THEME_UNKNOWN, SDL_SYSTEM_THEME_DARK}) {
        setTestSystemTheme(theme);
        SDL_Event changed{};
        changed.type = SDL_EVENT_SYSTEM_THEME_CHANGED;
        if (!SDL_PushEvent(&changed) || !waitFor(host, [&] { return pixelIs(theme == SDL_SYSTEM_THEME_DARK); })) return false;
        if (bridge.readDisplay() != "0") return false;
    }
    std::cout << "PASS: initial appearance and live dark/light/unknown palette pixels through Device.observeDarkMode\n";
#endif
    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    focus.window.windowID = host.getWindowId();
    if (!SDL_PushEvent(&focus) || !pump(host)) {
        return false;
    }
    if (!click(host, bridge, "1", "1") || !click(host, bridge, "2", "12") || !click(host, bridge, "+", "12") ||
        !click(host, bridge, "3", "3") || !click(host, bridge, "=", "15")) {
        return false;
    }
    if (!keyboard(host, bridge, SDLK_ESCAPE, "0") || !keyboard(host, bridge, SDLK_4, "4") ||
        !keyboard(host, bridge, SDLK_2, "42") || !keyboard(host, bridge, SDLK_BACKSPACE, "4") ||
        !keyboard(host, bridge, SDLK_SLASH, "4") || !keyboard(host, bridge, SDLK_0, "0") ||
        !keyboard(host, bridge, SDLK_RETURN, "Error") || !keyboard(host, bridge, SDLK_ESCAPE, "0") ||
        !keyboard(host, bridge, SDLK_1, "1") || !keyboard(host, bridge, SDLK_2, "12") ||
        !keyboard(host, bridge, SDLK_PLUS, "12") || !keyboard(host, bridge, SDLK_3, "3") ||
        !keyboard(host, bridge, SDLK_RETURN, "15")) {
        return false;
    }
    auto* window = SDL_GetWindowFromID(host.getWindowId());
    if (!SDL_SetWindowSize(window, 520, 740) ||
        !waitFor(host, [&] { return host.getMetrics().width == 520 && host.getMetrics().height == 740; })) {
        return false;
    }
    if (bridge.readDisplay() != "15") {
        return false;
    }
    // Give the asynchronous Valdi layout/render update time to reach the surface.
    for (int i = 0; i < 12; ++i) {
        if (!pump(host)) {
            return false;
        }
    }
    if (screenshot && !SDL_SaveBMP(SDL_GetWindowSurface(window), screenshot)) {
        return false;
    }
    std::cout << "PASS: Valdi TSX calculator taps, keyboard, clear, backspace, divide-by-zero, resize; result="
              << bridge.readDisplay() << " density=" << host.getMetrics().pixelDensity << '\n';
    return true;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: calculator_linux /path/to/modules /path/to/font.ttf [--self-test [snapshot.bmp]]\n";
        return 2;
    }
    const bool testing = argc > 3 && std::string(argv[3]) == "--self-test";
    for (int cycle = 0; cycle < (testing ? 3 : 1); ++cycle) {
#ifdef VALDI_LINUX_THEME_TEST
        setTestSystemTheme(cycle == 0 ? SDL_SYSTEM_THEME_DARK : cycle == 1 ? SDL_SYSTEM_THEME_LIGHT : SDL_SYSTEM_THEME_UNKNOWN);
#endif
        auto bridge = std::make_shared<CalculatorBridge>();
        CalculatorClient client(bridge);
        int exitCode = 0;
        {
            ValdiLinux::ValdiWindowApplication application(client);
            ValdiLinux::WindowApplicationOptions options;
            options.rootComponentPath = "CalculatorApp@calculator/src/CalculatorApp";
            options.moduleDirectories = {argv[1]};
            options.fontPath = argv[2];
            options.window.title = "Valdi Calculator";
            options.window.width = 420;
            options.window.height = 640;
            auto opened = application.open(options, bridge->context(bridge));
            if (!opened) {
                std::cerr << opened.error() << '\n';
                exitCode = 1;
            } else if (testing) {
                if (!selfTest(*application.getHost(), *bridge, cycle == 0 && argc > 4 ? argv[4] : nullptr)) {
                    exitCode = 1;
                }
            } else {
                while (true) {
                    auto result = application.getHost()->pump();
                    if (!result) {
                        std::cerr << result.error() << '\n';
                        exitCode = 1;
                        break;
                    }
                    if (!result.value()) {
                        break;
                    }
                }
            }
            bridge->stop();
        }
        if (testing && bridge->disconnects != 1) {
            std::cerr << "Expected one component disconnect during runtime teardown, got " << bridge->disconnects
                      << '\n';
            return 1;
        }
        if (exitCode) {
            std::cerr << "Calculator cycle " << cycle << " failed\n";
            return exitCode;
        }
        std::cout << "runtime-cycle=" << cycle << " closed\n";
    }
    return 0;
}
