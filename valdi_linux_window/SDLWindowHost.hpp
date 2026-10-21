#pragma once

#include "snap_drawing/cpp/Layers/LayerRoot.hpp"
#include "valdi_core/cpp/Threading/TaskQueue.hpp"
#include "valdi_core/cpp/Utils/Result.hpp"
#include "valdi_core/cpp/Utils/Void.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace ValdiLinux {

struct WindowMetrics {
    int width = 0; // SDL window coordinates, also used for layout and input.
    int height = 0;
    int pixelWidth = 0;
    int pixelHeight = 0;
    float pixelDensity = 1;
    float displayScale = 1; // User content-scale preference; distinct from pixel density.
};

struct WindowOptions {
    std::string title = "Valdi Linux window";
    int width = 800;
    int height = 600;
    size_t maxFramebufferBytes = 128 * 1024 * 1024;
};

struct KeyEvent {
    uint32_t key = 0;
    uint32_t scanCode = 0;
    uint16_t modifiers = 0;
    bool down = false;
    bool repeat = false;
};

// All callbacks run synchronously on the SDL/main thread. Text is UTF-8;
// composition offsets count UTF-8 characters, following SDL, rather than bytes
// or Valdi/JavaScript UTF-16 code units. The editor adapter must convert them.
class WindowClient {
public:
    virtual ~WindowClient() = default;
    virtual void onResize(const WindowMetrics& metrics) {}
    virtual void onKey(const KeyEvent& event) {}
    virtual void onTextInput(const std::string& text) {}
    virtual void onTextEditing(const std::string& text, int32_t start, int32_t length) {}
    virtual void onFocusChanged(bool focused) {}
};

// Experimental, single-window, CPU-raster host. SDL owns presentation and may
// use an internal accelerated renderer for its window surface. The host never
// creates a SnapDrawing GPU context or manages a swapchain.
// Caller owns the root, queue, client and SDL main thread. They must outlive the
// host. While open, this host exclusively owns their listener slots and the SDL
// event loop. Detach/destroy the host before destroying the root or runtime.
class SDLWindowHost final : private snap::drawing::LayerRootListener {
public:
    SDLWindowHost(const Valdi::Ref<snap::drawing::LayerRoot>& root, WindowClient& client,
                  Valdi::TaskQueue* mainQueue = nullptr);
    ~SDLWindowHost();
    SDLWindowHost(const SDLWindowHost&) = delete;
    SDLWindowHost& operator=(const SDLWindowHost&) = delete;

    Valdi::Result<Valdi::Void> open(const WindowOptions& options = {});
    // Process bounded batches of events/tasks and at most one scheduled frame.
    // false means a close request; failures contain the rendering/SDL error.
    Valdi::Result<bool> pump(int maxWaitMs = 16);
    void requestClose();

    Valdi::Result<Valdi::Void> setTextInput(bool enabled, int x, int y, int width, int height, int cursorOffset);
    Valdi::Result<std::string> getClipboardText() const;
    Valdi::Result<Valdi::Void> setClipboardText(const std::string& text);

    const WindowMetrics& getMetrics() const;
    uint32_t getWindowId() const;
    uint64_t getPresentedFrames() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
    void onNeedsProcessFrame(snap::drawing::LayerRoot& root) override;
    void onDidDraw(snap::drawing::LayerRoot& root, const Valdi::Ref<snap::drawing::DisplayList>& displayList,
                   const snap::drawing::CompositorPlaneList* planeList) override;
};

} // namespace ValdiLinux
