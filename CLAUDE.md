# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Project

OBS Studio 32+ video filter that automatically frames (crops + zooms) a webcam around detected faces or bodies.
Windows x64 only, Direct3D 11 renderer, NVIDIA AR SDK (Maxine) for detection. The goal is minimal CPU load:
frames never leave the GPU, only detection boxes reach the CPU.

## Build

Built with the obs-plugintemplate CMake setup; the user builds locally from "Developer PowerShell for VS 2022"
(VS 2022 Build Tools, no IDE). Do not run builds yourself unless asked.

```powershell
cmake --preset windows-x64                          # first run downloads OBS 32.0.0 sources + obs-deps into .deps/
cmake --build --preset windows-x64
cmake --install build_x64 --config RelWithDebInfo   # -> C:\ProgramData\obs-studio\plugins\obs-face-autoframe
```

- `buildspec.json` pins obs-studio **32.0.0**: OBS refuses plugins built against a newer OBS than the running one,
  so headers must match the oldest supported version.
- CI uses the `windows-ci-x64` preset (Windows SDK available on GitHub runners); see `.github/workflows/`.
- Framing unit tests are a standalone project without OBS: `tests/CMakeLists.txt` (Catch2 v3 via `find_package`).

## Layout

```
src/
  plugin-main.cpp        module load: SDK runtime + filter registration
  filter/                OBS filter: settings, properties UI, render/tick orchestration
  detect/                detector contract (IDetector), worker thread (AsyncDetector), backend contract
    nvar/                NVIDIA AR SDK: runtime loader, own API declarations, BoxDetector, factory, GPU check
  framing/               pure C++ (no OBS / SDK includes): tracker, target selector, framer, camera motion
  render/                source capture, detection frame (GPU downscale + tonemap), debug drawing
  util/                  graphics guard, logging, localization helpers
data/effects, data/locale  shader for the detection frame, en-US / ru-RU strings
tests/framing/           Catch2 tests for src/framing
```

## Conventions

- C++20, `.clang-format` (tabs, 120 columns). Code, comments, commit messages and docs are in English.
- `src/framing/` must stay free of OBS and NVIDIA dependencies so it remains unit-testable; add tests for changes.
- The NVIDIA SDK is loaded at runtime (`LoadLibraryExW`); only the used subset is declared in `nvar-defs.hpp`.
  End users install the NVIDIA AR SDK redistributable; no SDK headers are vendored.
- Every property has a long description (`obs_property_set_long_description`) and strings in both locale files.
- Comments explain why, not what. No temporary planning references (phase numbers, task ids) in code.

## OBS / GPU pitfalls already solved — keep them solved

- `video_render` can run several times per frame (preview, program, projectors): capture and detect once per tick.
- `video_tick` runs **outside** the graphics lock; `update()` holds it. Do not touch settings or GPU state in tick.
- OBS resets effect parameters to defaults when a technique ends: set them before every draw (debug colors, scale
  effect dimensions).
- `gs_texrender` recreates its texture on resize: the CUDA registration follows the `ID3D11Texture2D` pointer.
- Device loss: `gs_register_loss_callbacks` — the detector is destroyed before OBS frees textures.
- `get_width` / `get_height` are called from any thread: output size is stored in atomics.
- The detection frame has a fixed size per source/resolution (the frame area is letterboxed into it), because a size
  change forces an `NvAR_Load`.
- `plugin-support.h` redeclares `blogva`; include it only through `util/log.hpp` (after libobs headers).
- The camera dead zone is measured on the unclamped desired position, so a target pinned to the frame-area edge is
  still reached.
