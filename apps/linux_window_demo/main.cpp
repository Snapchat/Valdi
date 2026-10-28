#include "valdi_linux_window/SDLWindowHost.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Layers/TextLayer.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Touches/SingleTapGestureRecognizer.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace sd = snap::drawing;

namespace {
Valdi::Result<Valdi::BytesView> readFont(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (!file || size <= 0 || size > 8 * 1024 * 1024) {
        return Valdi::Error("Invalid font file (limit: 8 MiB)");
    }
    auto bytes = Valdi::makeShared<Valdi::Bytes>();
    bytes->resize(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes->data()), size)) {
        return Valdi::Error("Unable to read font");
    }
    return Valdi::BytesView(bytes);
}

class DemoClient final : public ValdiLinux::WindowClient {
public:
    Valdi::Ref<sd::TextLayer> label;
    Valdi::Ref<sd::Layer> card;
    std::string committed;
    std::string preedit;
    int taps = 0;

    void update() {
        label->setText(STRING_LITERAL("SDL3 + SnapDrawing\n\n") +
                       Valdi::StringBox::fromString("Tap count: " + std::to_string(taps) +
                                                    "\n\nText input probe (not an editor):\n" + committed +
                                                    "\nComposition: " + preedit));
    }
    void onResize(const ValdiLinux::WindowMetrics& metrics) override {
        card->setFrame(sd::Rect::makeXYWH(24, 24, std::max(1, metrics.width - 48), std::max(1, metrics.height - 48)));
        label->setFrame(sd::Rect::makeXYWH(24, 24, std::max(1, metrics.width - 96), std::max(1, metrics.height - 96)));
    }
    void onTextInput(const std::string& text) override {
        if (committed.size() + text.size() <= 4096) {
            committed += text;
        }
        preedit.clear();
        update();
    }
    void onTextEditing(const std::string& text, int32_t, int32_t) override {
        // Keep whole UTF-8 sequences when applying the demo's text limit.
        preedit = text.size() <= 4096 ? text : "[composition too long]";
        update();
    }
};
} // namespace

int main(int argc, char** argv) {
    // Explicit font input avoids platform font-discovery policy in the host.
    if (argc < 2) {
        std::cerr << "Usage: linux_window_demo /path/to/font.ttf [duration-seconds] [snapshot.bmp]\n";
        return 2;
    }
    auto fontData = readFont(argv[1]);
    if (!fontData) {
        std::cerr << fontData.error() << '\n';
        return 1;
    }
    auto fonts = Valdi::makeShared<sd::FontManager>(Valdi::ConsoleLogger::getLogger());
    const sd::FontStyle style(sd::FontWidthNormal, sd::FontWeightNormal, sd::FontSlantUpright);
    fonts->registerTypeface(STRING_LITERAL("Demo"), style, true, fontData.value());
    auto font = fonts->getFontWithFamilyNameAndStyle(STRING_LITERAL("Demo"), style, 22, 1, false);
    if (!font) {
        std::cerr << font.error() << '\n';
        return 1;
    }
    auto resources = Valdi::makeShared<sd::Resources>(fonts, 1, Valdi::ConsoleLogger::getLogger());
    auto root = Valdi::makeShared<sd::LayerRoot>(resources);
    auto content = sd::makeLayer<sd::Layer>(resources);
    content->setBackgroundColor(sd::Color::make(0xff102337));
    DemoClient client;
    client.card = sd::makeLayer<sd::Layer>(resources);
    client.card->setBackgroundColor(sd::Color::make(0xffe9f2f5));
    client.label = sd::makeLayer<sd::TextLayer>(resources);
    client.label->setNumberOfLines(0);
    client.label->setTextFont(font.value());
    client.label->setTextColor(sd::Color::make(0xff102337));
    client.card->addChild(client.label);
    content->addChild(client.card);
    auto tap = Valdi::makeShared<sd::SingleTapGestureRecognizer>(resources->getGesturesConfiguration());
    tap->setListener([&client](const auto&, auto state, const auto&) {
        if (state == sd::GestureRecognizerStateEnded) {
            ++client.taps;
            client.update();
        }
    });
    client.card->addGestureRecognizer(tap);
    client.update();
    root->setContentLayer(content, sd::ContentLayerSizingModeMatchSize);
    int exitCode = 0;
    {
        ValdiLinux::SDLWindowHost host(root, client);
        auto opened = host.open();
        if (!opened) {
            std::cerr << opened.error() << '\n';
            exitCode = 1;
        } else {
            auto input = host.setTextInput(true, 48, 180, 600, 32, 0);
            if (!input) {
                std::cerr << input.error() << '\n';
            }
            const auto start = std::chrono::steady_clock::now();
            const int duration = argc > 2 ? std::atoi(argv[2]) : 0;
            while (true) {
                auto running = host.pump();
                if (!running) {
                    std::cerr << running.error() << '\n';
                    exitCode = 1;
                    break;
                }
                if (!running.value()) {
                    break;
                }
                if (duration > 0 && std::chrono::steady_clock::now() - start > std::chrono::seconds(duration)) {
                    break;
                }
            }
            std::cout << "Presented " << host.getPresentedFrames() << " frames, " << host.getMetrics().pixelWidth << 'x'
                      << host.getMetrics().pixelHeight << " pixels\n";
            if (argc > 3 && !SDL_SaveBMP(SDL_GetWindowSurface(SDL_GetWindowFromID(host.getWindowId())), argv[3])) {
                std::cerr << SDL_GetError() << '\n';
                exitCode = 1;
            }
        }
    }
    root->destroy();
    return exitCode;
}
