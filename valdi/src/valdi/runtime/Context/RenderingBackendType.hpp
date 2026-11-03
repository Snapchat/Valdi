//
//  RenderingBackendType.hpp
//  valdi-desktop-apple
//
//  Created by Ramzy Jaber on 8/16/22.
//

#pragma once

#include "valdi_core/cpp/Utils/StringCache.hpp"

namespace Valdi {

enum RenderingBackendType {
    RenderingBackendTypeNative,
    RenderingBackendTypeSnapDrawing,
    RenderingBackendTypeTerminal,
    RenderingBackendTypeUnset
};

const StringBox& getBackendString(RenderingBackendType type);

} // namespace Valdi
