# Third-party notices for the experimental Linux window host

## SDL3

This module builds SDL 3.4.18 from the [Bazel Central Registry](https://github.com/bazelbuild/bazel-central-registry/tree/main/modules/sdl3/3.4.18).
The release archive and registry overlays are verified by Bazel's integrity hashes.
The upstream [zlib license](https://github.com/libsdl-org/SDL/blob/release-3.4.18/LICENSE.txt)
is reproduced unchanged below:

```text
Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

```

## Linux backend dependencies

The BCR SDL BUILD uses a headers-only adapter for these Linux libraries; SDL loads
them dynamically by soname when the corresponding subsystem/backend is selected.
The offscreen software-surface test initializes video only and does not require
the desktop or audio libraries. The validated Linux x86_64 offscreen test links
SDL/SnapDrawing statically; its ELF runtime dependencies are `libm.so.6` and
`libc.so.6` (plus the dynamic loader). Desktop backends can still load the
libraries below on demand. This is a build/backend inventory, not a packaged
SDK license audit. Distributors must retain the notices for their actual binaries,
including Valdi/SnapDrawing and any transitively linked or runtime-loaded libraries.

| Registry module/version requested by SDL | Runtime library | Upstream license/notices |
| --- | --- | --- |
| `alsa_lib` 1.2.9.bcr.4 | `libasound.so.2` (audio) | [LGPL-2.1-or-later and file-specific notices](https://github.com/alsa-project/alsa-lib/blob/v1.2.9/COPYING) |
| `libx11` 1.8.12.bcr.5 | `libX11.so.6` | [X11/MIT notices](https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/libX11-1.8.12/COPYING) |
| `libxcursor` 1.2.3 | `libXcursor.so.1` | [X11/MIT notices](https://gitlab.freedesktop.org/xorg/lib/libxcursor/-/blob/libXcursor-1.2.3/COPYING) |
| `libxext` 1.3.6 | `libXext.so.6` | [X11/MIT notices](https://gitlab.freedesktop.org/xorg/lib/libxext/-/blob/libXext-1.3.6/COPYING) |
| `libxfixes` 6.0.2 | `libXfixes.so.3` | [X11/MIT notices](https://gitlab.freedesktop.org/xorg/lib/libxfixes/-/blob/libXfixes-6.0.2/COPYING) |
| `libxi` 1.8.2 | `libXi.so.6` | [X11/MIT notices](https://gitlab.freedesktop.org/xorg/lib/libxi/-/blob/libXi-1.8.2/COPYING) |
| `libxrandr` 1.5.4 | `libXrandr.so.2` | [X11/MIT notices](https://gitlab.freedesktop.org/xorg/lib/libxrandr/-/blob/libXrandr-1.5.4/COPYING) |
| `wayland` 1.24.0.bcr.1 | `libwayland-client.so.0`, `libwayland-cursor.so.0`, `libwayland-egl.so.1` | [MIT notices](https://gitlab.freedesktop.org/wayland/wayland/-/blob/1.24.0/COPYING) |
| `xkbcommon` 1.9.2.bcr.beta.1 | `libxkbcommon.so.0` | [MIT/X11 notices](https://github.com/xkbcommon/libxkbcommon/blob/xkbcommon-1.9.2/LICENSE) |

SDL also builds its bundled third-party sources; their original notices remain in
the verified archive. Registry dependencies include build tools and header-generation
inputs such as `rules_cc_autoconf`, Wayland protocols, Xorg protocol headers and
`libffi`. Bazel's module graph/lockfile records resolved versions; a declared module
does not imply its library is linked into the deployed host.

Interactive presentation can load EGL/OpenGL implementations and graphics drivers
(e.g. Mesa), whose licenses depend on the installed distribution. Fontconfig is a
future RFC 0001 integration and is not added by this module. See the repository's
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) for Valdi's existing dependencies.
