#pragma once

#include <memory>

namespace snap::valdi_core {
class HTTPRequestManager;
}

namespace Valdi {

// requestManager is null for an application built without an HTTP backend. Not defaulted, so that
// each cli_main template says which one it is.
int valdiCLIRun(const char* scriptPath,
                int argc,
                const char** argv,
                const std::shared_ptr<snap::valdi_core::HTTPRequestManager>& requestManager);

}
