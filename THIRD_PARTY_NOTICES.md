# Third-Party Software

Valdi includes or interacts with third-party software. A detailed report of the third-party software used, including their respective licenses, can be found at the following link.

https://portal.fossa.com/p/snap/release/2857/23669

Third-party code vendored in this repository keeps its original license, which can be found alongside that code (for example, under `third-party/`).

We aim to maintain transparency and compliance with all third-party license requirements. If you spot an issue or believe attribution is missing, please reach out to us at opensource@snap.com.

## Code adapted from third-party projects

The terminal renderer (`//valdi:valdi_terminal`, under `valdi/src/valdi/terminal/`) and the `valdi_tui` module (under `src/valdi_modules/src/valdi/valdi_tui/`) contain code adapted from the following projects. They are distributed under the MIT license reproduced below, and the same notice is kept in the source files that carry the adapted code.

| Project | Source | What is adapted | Where |
|---------|--------|-----------------|-------|
| ansi-regex | https://github.com/chalk/ansi-regex | Escape sequence matcher | `valdi/src/valdi/terminal/Paint/Ansi.hpp`, `Ansi.cpp`; `src/valdi_modules/src/valdi/valdi_tui/src/ansi.ts` |
| ansi-styles | https://github.com/chalk/ansi-styles | RGB to ANSI 256 / ANSI 16 conversions, color names and levels | `valdi/src/valdi/terminal/Paint/Ansi.hpp`, `Ansi.cpp`; `src/valdi_modules/src/valdi/valdi_tui/src/ansi.ts` |
| cli-boxes | https://github.com/sindresorhus/cli-boxes | Border character sets | `valdi/src/valdi/terminal/Paint/Borders.hpp`, `Borders.cpp` |
| log-update, ansi-escapes | https://github.com/sindresorhus/log-update, https://github.com/sindresorhus/ansi-escapes | Erase and redraw pattern for the live frame | `valdi/src/valdi/terminal/Paint/FrameWriter.hpp`, `FrameWriter.cpp` |

```
MIT License

Copyright (c) Sindre Sorhus <sindresorhus@gmail.com> (https://sindresorhus.com)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
