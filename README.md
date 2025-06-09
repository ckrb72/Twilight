# Twilight


## Build
This project uses CMake so make sure you have that before you try to build. If you don't have it you can download it here: https://cmake.org/download/

Once you have that installed you can use whatever compiler / build system cmake supports.

For example, if you want to build using mingw (like I do), just do the following:

```
mkdir build
cd build
cmake ../ -G "MinGW Makefiles"
mingw32-make
```