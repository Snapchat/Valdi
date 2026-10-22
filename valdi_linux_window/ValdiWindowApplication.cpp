#include "valdi_linux_window/ValdiWindowApplication.hpp"

#include "valdi/linux/LinuxRuntime.hpp"
#include "valdi/runtime/Context/ViewManagerContext.hpp"
#include "valdi/runtime/JavaScript/JavaScriptRuntime.hpp"
#include "valdi/runtime/Runtime.hpp"
#include "valdi/snap_drawing/Layers/ValdiLayer.hpp"
#include "valdi/snap_drawing/Runtime.hpp"
#include "valdi/standalone_runtime/StandaloneMainQueue.hpp"
#include "valdi/standalone_runtime/StandaloneResourceLoader.hpp"
#include "valdi_core/cpp/Utils/Bytes.hpp"
#include "valdi_core/cpp/Utils/ConsoleLogger.hpp"
#include "snap_drawing/cpp/Resources.hpp"
#include "snap_drawing/cpp/Drawing/IFrameScheduler.hpp"
#include "snap_drawing/cpp/Text/FontManager.hpp"
#include "snap_drawing/cpp/Touches/GesturesConfiguration.hpp"

#include <fstream>

namespace ValdiLinux {

struct ValdiWindowApplication::Impl {
    WindowClient& client;
    Valdi::Ref<Valdi::ValdiStandaloneRuntime> runtime;
    Valdi::Ref<snap::drawing::Runtime> drawing;
    Valdi::Ref<Valdi::ViewManagerContext> viewContext;
    Valdi::Ref<snap::drawing::ValdiLayer> component;
    Valdi::Ref<snap::drawing::LayerRoot> root;
    std::unique_ptr<SDLWindowHost> host;

    explicit Impl(WindowClient& client) : client(client) {}

    ~Impl() {
        // Detach SDL/queue callbacks before releasing any runtime resources.
        host = nullptr;
        if (root != nullptr) {
            root->destroy();
        }
        component = nullptr;
        root = nullptr;
        viewContext = nullptr;
        // ViewManagerContext refers to the drawing view manager. Keep the
        // drawing runtime alive while Valdi tears down its remaining contexts.
        runtime = nullptr;
        drawing = nullptr;
    }
};

ValdiWindowApplication::ValdiWindowApplication(WindowClient& client) : _impl(std::make_unique<Impl>(client)) {}
ValdiWindowApplication::~ValdiWindowApplication() = default;

Valdi::Result<Valdi::Void> ValdiWindowApplication::open(const WindowApplicationOptions& options,
                                                        const Valdi::Value& componentContext) {
    auto& p = *_impl;
    if (p.runtime != nullptr) {
        return Valdi::Error("Window application can only be opened once");
    }
    if (options.rootComponentPath.empty()) {
        return Valdi::Error("A root component path is required");
    }
    std::ifstream file(options.fontPath, std::ios::binary | std::ios::ate);
    const auto length = file.tellg();
    if (!file || length <= 0 || length > 8 * 1024 * 1024) {
        return Valdi::Error("A readable font file of at most 8 MiB is required");
    }
    auto fontBytes = Valdi::makeShared<Valdi::Bytes>();
    fontBytes->resize(static_cast<size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(fontBytes->data()), length)) {
        return Valdi::Error("Unable to read font");
    }

    p.runtime = createLinuxRuntime(false, true);
    for (const auto& directory : options.moduleDirectories) {
        p.runtime->getResourceLoader().addModuleSearchDirectory(Valdi::StringBox::fromString(directory));
    }
    p.runtime->setupJsRuntime({});
    p.drawing = Valdi::makeShared<snap::drawing::Runtime>(
        nullptr, snap::drawing::GesturesConfiguration::getDefault(), nullptr, nullptr,
        Valdi::ConsoleLogger::getLogger(), p.runtime->getRuntimeManager().getWorkerQueue(), 32 * 1024 * 1024);
    p.drawing->initializeViewManager(Valdi::PlatformTypeLinux);
    p.drawing->registerAssetLoaders(*p.runtime->getRuntimeManager().getAssetLoaderManager());
    p.drawing->getViewManager()->registerTypeface(Valdi::StringBox::fromString(options.fontFamily),
                                                  snap::drawing::FontStyle(snap::drawing::FontWidthNormal,
                                                                           snap::drawing::FontWeightNormal,
                                                                           snap::drawing::FontSlantUpright),
                                                  true, Valdi::BytesView(fontBytes));
    p.viewContext = p.runtime->getRuntimeManager().createViewManagerContext(*p.drawing->getViewManager(), false);
    p.runtime->getRuntime().getJavaScriptRuntime()->setDefaultViewManagerContext(p.viewContext);
    p.root = Valdi::makeShared<snap::drawing::LayerRoot>(p.drawing->getResources());
    p.component = snap::drawing::makeLayer<snap::drawing::ValdiLayer>(p.drawing->getResources(),
                                                                      &p.runtime->getRuntime(), p.viewContext.get());
    p.component->setComponent(Valdi::StringBox::fromString(options.rootComponentPath), Valdi::Value(),
                              componentContext);
    p.root->setContentLayer(p.component, snap::drawing::ContentLayerSizingModeMatchSize);
    p.host = std::make_unique<SDLWindowHost>(p.root, p.client, p.runtime->getMainQueue().get());
    return p.host->open(options.window);
}

SDLWindowHost* ValdiWindowApplication::getHost() const { return _impl->host.get(); }

} // namespace ValdiLinux
