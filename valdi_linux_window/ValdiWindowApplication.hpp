#pragma once

#include "valdi_linux_window/SDLWindowHost.hpp"
#include "valdi_core/cpp/Utils/Value.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ValdiLinux {

struct WindowApplicationOptions {
    WindowOptions window;
    std::string rootComponentPath;
    std::vector<std::string> moduleDirectories;
    std::string fontPath;
    std::string fontFamily = "Window";
};

// Experimental prototype API; its lifecycle contract is still under RFC review
// (see README.md). Owns the standalone runtime, drawing context, component and
// SDL host. No application services are installed. All methods and destruction
// run on the main thread.
class ValdiWindowApplication {
public:
    explicit ValdiWindowApplication(WindowClient& client);
    ~ValdiWindowApplication();
    ValdiWindowApplication(const ValdiWindowApplication&) = delete;
    ValdiWindowApplication& operator=(const ValdiWindowApplication&) = delete;

    Valdi::Result<Valdi::Void> open(const WindowApplicationOptions& options,
                                    const Valdi::Value& componentContext = Valdi::Value());
    SDLWindowHost* getHost() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace ValdiLinux
