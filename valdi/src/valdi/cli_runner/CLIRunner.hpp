#pragma once

#include <memory>

namespace snap::valdi_core {
class HTTPRequestManager;
}

namespace Valdi {

int valdiCLIRun(const char* scriptPath,
                int argc,
                const char** argv,
                const std::shared_ptr<snap::valdi_core::HTTPRequestManager>& requestManager = nullptr);

}
