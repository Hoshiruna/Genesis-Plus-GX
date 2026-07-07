Compile with MinGW.
You will also need to install the SDL library (http://www.libsdl.org/).
Zlib is required for zipped rom support.

For the SDL3 frontend on Windows, install SDL3 and zlib in your MinGW/MSYS2
environment, then build from this directory with:

  make -f Makefile.sdl3

Please distribute required dlls with the executable.
