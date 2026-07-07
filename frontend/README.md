# Genesis Plus GX Frontend

This directory adds a small C frontend interface around the existing emulator core. The core still owns emulation state and timing-sensitive behavior; a host frontend supplies platform video, audio, input, and logging callbacks.

## Files

- `frontend_host.h`: callback contract for video, audio, input, and logging.
- `emu_runner.c` / `emu_runner.h`: emulator lifecycle wrapper used by frontends.
- `osd.h` / `main.h`: frontend-specific OSD glue for the existing core include path.
- `sdl3_frontend.c`: SDL3 host using the interface.
- `CMakeLists.txt`: CMake build for the frontend.
- `CMakePresets.json`: Visual Studio 2022 x64 configure/build presets.
- `vstudio/genplus_frontend.sln`: legacy hand-written Visual Studio 2022 x64 solution.

## Visual Studio Setup With CMake

CMake is the recommended Visual Studio path. It builds the frontend core wrapper, builds the bundled zlib sources, and resolves SDL3 automatically.

From this directory:

```powershell
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64-release
```

The preset generates:

```text
frontend\build\vs2022-x64\genplus_frontend.sln
```

Open that generated solution in Visual Studio, or open the `frontend` folder directly in Visual Studio and select the `vs2022-x64` preset.

In the generated solution, `ALL_BUILD` and `ZERO_CHECK` are CMake utility targets, while `genplus_zlib` and `genplus_core_static` are libraries. Build `ALL_BUILD` or `genplus_frontend`, but set only `genplus_frontend` as the startup project when running under the debugger.

By default, CMake first tries:

```cmake
find_package(SDL3 CONFIG QUIET)
```

If SDL3 is not already available, it downloads the official Windows VC development package:

```text
https://www.libsdl.org/release/SDL3-devel-3.4.12-VC.zip
```

and imports `SDL3.lib` / `SDL3.dll` as `SDL3::SDL3`. The post-build step copies `SDL3.dll` beside the executable.

To use an existing SDL3 package instead of downloading, configure with `GENPLUS_FRONTEND_FETCH_SDL3=OFF` and provide SDL3 through `CMAKE_PREFIX_PATH` or a vcpkg toolchain file.

Run it, then use `File > Open ROM...` to choose a ROM. Passing a ROM path on the command line still works:

```text
frontend\build\vs2022-x64\Release\genplus_frontend.exe "C:\path\to\game.md"
```

## Sample SDL3 Controls

- Arrow keys: D-pad
- `A`, `S`, `D`: Genesis `A`, `B`, `C`
- `Q`, `W`, `E`: Genesis `X`, `Y`, `Z`
- Enter: Start
- Backspace: Mode
- Tab: reset
- F2: fullscreen
- Esc: quit

## Porting Another Frontend

Implement `sim_frontend_host_t` callbacks, call `sim_emu_init`, then `sim_emu_load_rom`, and run `sim_emu_run_frame` from your frame loop. Use `sim_emu_shutdown` before exit so SRAM/BRAM is flushed.
