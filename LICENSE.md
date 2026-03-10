# License

SPDX-License-Identifier: MIT AND LGPL-2.1-or-later

fcsim's own source code is MIT. The project also contains one component
(`src/fpmath/atan2.c`) under LGPL-2.1-or-later. See that file's header and
`ATTRIBUTION.md` for full provenance details on all third-party components.

---

## fcsim

Copyright (c) 2022 fcsim contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

## Third-party components

### Box2D (`src/box2d/`, `include/box2d/`)

Version 1.4.3. Copyright (c) 2006-2007 Erin Catto http://www.gphysics.com

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use
of this software. Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

---

### OpenGL API header (`src/gl.h`)

Copyright 2013-2020 The Khronos Group Inc.
SPDX-License-Identifier: MIT

---

### atan2 (`src/fpmath/atan2.c`)

SPDX-License-Identifier: LGPL-2.1-or-later

IBM Accurate Mathematical Library, written by International Business Machines Corp.
Copyright (C) 2001-2018 Free Software Foundation, Inc.

This file is part of the GNU C Library. It is free software; you can
redistribute it and/or modify it under the terms of the GNU Lesser General
Public License as published by the Free Software Foundation; either version 2.1
of the License, or (at your option) any later version. See the file itself for
the full license text, or visit <https://www.gnu.org/licenses/>.

As fcsim is distributed with full source code, the requirement to provide the
means to relink against a modified version of this component is satisfied.

---

### sin/cos (`src/fpmath/sincos.c`)

This implementation was received as pre-existing code of unknown provenance.
Forensic analysis (see `ATTRIBUTION.md`) found that its constants and lookup
table match bit-for-bit those found in Adobe Flash Player 10.0.32. It may
originate directly from Flash Player or from a binary downstream of it.

This code is included specifically to reproduce the numerical behaviour of
Fantastic Contraption as it ran under Adobe Flash Player -- substituting any
other implementation would change the floating-point results and break the
preservation goal of this project. Adobe Flash Player reached end-of-life on
2020-12-31.

If you are the copyright holder of this implementation and have concerns about
its inclusion, please open an issue.
