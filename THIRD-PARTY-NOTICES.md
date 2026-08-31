# Third-Party Notices

QRO20 is licensed under the GNU General Public License v3.0 (see `LICENSE`).
It builds against, and in one case vendors, the following third-party
components, which remain under their own licenses:

## wxWidgets 3.2.6

GUI framework. Not vendored in this repo (installed separately per the build
instructions in `CLAUDE.md`). Licensed under the **wxWindows Library Licence**
(a modified LGPL that explicitly permits static linking and distribution of
the resulting binary without requiring the linked application to be LGPL).
See https://www.wxwidgets.org/about/licence/ for the full text.

## PortAudio

Real-time audio I/O library. The `portaudio_x64.dll`/`.lib`,
`portaudio_x86.dll`/`.lib`, and `portaudio.h` files are vendored directly in
this repository's root. PortAudio is distributed under an MIT-style license:

```
PortAudio Portable Real-Time Audio Library
Copyright (c) 1999-2006 Ross Bencina and Phil Burk

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

(Full text: https://github.com/PortAudio/portaudio/blob/master/LICENSE.txt)

## Intel oneAPI IPP

Used for DSP primitives (FFT, DFT, resampling). **This is proprietary Intel
software, not open source**, and is not vendored in this repository — it
must be installed separately from
`C:\Program Files (x86)\Intel\oneAPI\ipp\latest\` per `CLAUDE.md`. Its use
and any redistribution of its runtime DLLs is governed solely by Intel's own
Intel Simplified Software License, not by this project's GPL-3.0 license.
See https://www.intel.com/content/www/us/en/developer/articles/license/onemkl-license-faq.html
for Intel's redistribution terms if you plan to ship IPP runtime files with
a built copy of QRO20.
