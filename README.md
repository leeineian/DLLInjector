# DLL Injector

A lightweight, zero-dependency UWP DLL Injector for Windows.

### Windows Defender / Antivirus Warning
Because this tool writes memory and spawns threads in another process (process injection), antivirus software might flag it as a virus. This is a false positive—feel free to inspect the source code and compile it yourself.

## Building from Source

### Requirements
- CMake 3.20+
- MSVC compiler (Visual Studio 2019/2022) or MinGW

### Steps
1. Open terminal in the directory.
2. Run:
   ```cmd
   cmake -B build
   cmake --build build --config Release
   ```
3. The compiled executable `DLLInjector.exe` will be located in `build/src/Release/`.
