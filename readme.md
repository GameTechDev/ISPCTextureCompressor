# Fast ISPC Texture Compressor

State of the art texture compression for BC6H, BC7, ETC1, ASTC and BC1/BC3.

Uses [ISPC compiler](https://ispc.github.io/).

See [Fast ISPC Texture Compressor](https://software.intel.com/en-us/articles/fast-ispc-texture-compressor-update) post on
Intel Developer Zone.

Supported compression formats:

* BC6H (FP16 HDR input)
* BC7
* ASTC (LDR, block sizes up to 8x8)
* ETC1
* BC1, BC3 (aka DXT1, DXT5)

## License

Copyright 2017 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Contributing

Please see
[CONTRIBUTING](https://github.com/GameTechDev/ISPCTextureCompressor/blob/master/contributing.md)
for information on how to request features, report issues, or contribute code
changes.

## Build Instructions

* Windows:
	* Use Visual Studio 2012 on later, build solution or project files.
	* ISPC version 1.8.2 is included in this repo.

* Mac OS X:
	* Xcode project file included only for compressor itself, not for the examples.
	* You'll need to get ISPC compiler version [1.8.2 build](https://sf.net/projects/ispcmirror) and put the compiler executable into `ISPC Texture Compressor/ispc_osx`.
	* Use `ISPC Texture Compressor/ispc_texcomp.xcodeproj`, tested with Xcode 7.3.
	* Minimum OS X deployment version set to 10.9.
	* dylib install name is set to `@executable_path/../Frameworks/$(EXECUTABLE_PATH)`

* Linux:
	* Makefile included only for compressor itself, not for the examples.
	* You'll need to get ISPC compiler version [1.8.2 build](https://sf.net/projects/ispcmirror) and put the compiler executable into `ISPC Texture Compressor/ispc_linux`.
	* `make -f Makefile.linux` from `ISPC Texture Compressor` folder.
