#pragma once

#include <string>
#include <string_view>

#include "snap_drawing/cpp/Text/FontManager.hpp"

namespace snap::drawing {

Ref<Font> loadTestFont(const Ref<FontManager>& fontManager,
                       std::string_view fontFamilyName,
                       FontStyle fontStyle,
                       const std::string& filename,
                       bool canUseAsFallback = false,
                       Scalar fontSize = 17,
                       double scale = 1.0);

} // namespace snap::drawing
