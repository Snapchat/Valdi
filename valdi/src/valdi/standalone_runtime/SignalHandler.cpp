//
//  SignalHandler.cpp
//  valdi-pc
//
//  Created by Simon Corsin on 20/12/22.
//

#ifndef __ANDROID__

#include "valdi/standalone_runtime/SignalHandler.hpp"
#include "backward.hpp"

namespace Valdi {

void SignalHandler::install() {
    static backward::SignalHandling kSignalHandle;
}

} // namespace Valdi

#endif
