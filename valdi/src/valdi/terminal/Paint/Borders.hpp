//
//  Borders.hpp
//
//  The border character sets are ported from cli-boxes, distributed under the MIT license:
//
//  MIT License
//
//  Copyright (c) Sindre Sorhus <sindresorhus@gmail.com> (https://sindresorhus.com)
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
//  associated documentation files (the "Software"), to deal in the Software without restriction,
//  including without limitation the rights to use, copy, modify, merge, publish, distribute,
//  sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all copies or
//  substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//  NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#pragma once

#include <string_view>

namespace Valdi::Terminal {

/** Characters used to draw a box border. */
struct BorderCharacters {
    std::string_view topLeft;
    std::string_view top;
    std::string_view topRight;
    std::string_view right;
    std::string_view bottomRight;
    std::string_view bottom;
    std::string_view bottomLeft;
    std::string_view left;
};

/**
 * Resolves a border style name: single, double, round, bold, singleDouble, doubleSingle, classic or arrow.
 * Returns nullptr for unknown names.
 */
const BorderCharacters* resolveBorderStyle(std::string_view name);

} // namespace Valdi::Terminal
