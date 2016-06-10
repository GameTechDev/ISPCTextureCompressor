# Fast ISPC Texture Compressor

State of the art texture compression for BC6H, BC7, ETC1 and ASTC.

Uses [ISPC compiler](https://ispc.github.io/).


## Build Instructions

* Windows:
** Use Visual Studio 2012 on later, build solution or project files.
** ISPC version 1.8.2 is included in this repo.
* Mac OS X:
** Xcode project file included only for compressor itself, not for the examples.
** You'll need to get ISPC compiler version [1.8.2 build](https://sf.net/projects/ispcmirror) and put the compiler executable into `ISPC Texture Compressor/ispc_osx`.
** Use `ISPC Texture Compressor/ispc_texcomp.xcodeproj`, tested with Xcode 7.3.
** Minimum OS X deployment version set to 10.9.
** dylib install name is set to `@executable_path/../Frameworks/$(EXECUTABLE_PATH)`
* Linux:
** Makefile included only for compressor itself, not for the examples.
** You'll need to get ISPC compiler version [1.8.2 build](https://sf.net/projects/ispcmirror) and put the compiler executable into `ISPC Texture Compressor/ispc_linux`.
** `make -f Makefile.linux` from `ISPC Texture Compressor` folder.
