# StlThumbnail

Windows Explorer thumbnail handler for **.stl** files (ASCII + binary), with a
WPF helper app to configure how previews are rendered: camera angle, model
color, background, and lighting.

## Components

| Path | What |
|---|---|
| `src/` | Native COM DLL: `IThumbnailProvider` + software rasterizer (no dependencies) |
| `config-app/` | WPF helper app (`StlThumbConfig.exe`) with live preview |
| `test/` | Test STL generators, render harness, shell-pipeline verifier |
| `build.cmd` | Builds `build/StlThumbnail.dll` with MSVC (x64) |
| `install.cmd` / `uninstall.cmd` | Per-user registration (no admin required) |

## Build & install

```
build.cmd                                   :: native DLL  -> build\StlThumbnail.dll
cd config-app && dotnet build -c Release    :: helper app (framework-dependent, dev use)
install.cmd                                 :: register for current user
```

### Zero-dependency package for other machines

```
publish.cmd
```

produces `dist\` — copy that folder to **any Windows 11 x64 machine** and run
`install.cmd` there. No .NET runtime, no VC++ redistributable, no admin:

- `StlThumbConfig.exe` — self-contained single-file publish (~133 MB, runtime bundled)
- `StlThumbnail.dll` — statically linked CRT (`/MT`); imports only inbox DLLs
  (shlwapi, gdi32, advapi32, shell32, kernel32)
- `sample.stl`, `install.cmd`, `uninstall.cmd`

Explorer immediately starts using the handler for `.stl` files in any icon
view. Existing thumbnails are cached by Windows; press **F5** in the folder or
clear the thumbnail cache (`cleanmgr` → Thumbnails) to force regeneration.

## Configuration

Run `StlThumbConfig.exe`. All settings render a **live preview** (open any STL
as the sample). *Save & Apply* writes to `HKCU\Software\StlThumbnail`, which
the COM handler reads on every thumbnail request, and notifies Explorer.

Settings: yaw / pitch (camera), model color, background color or transparent,
ambient + key light intensity, key light direction/height.

## Architecture notes

- The renderer is a small self-contained software rasterizer (`src/renderer.cpp`):
  z-buffered triangle fill, flat shading with recomputed face normals (stored
  STL normals are unreliable), model auto-fit with 6% margin.
- The same DLL exports a flat C API `StlRenderToFile` used by both the helper
  app (P/Invoke) for its preview and the test harness — so the preview is
  pixel-identical to what Explorer shows.
- Registration is per-user (`HKCU\Software\Classes`), CLSID
  `{5E9BBB1E-9B37-4E22-A0F0-3D8B62D3C10A}`, bound to
  `.stl\ShellEx\{E357FCCD-A995-4576-B01F-234630154E96}`.
- Binary STLs starting with "solid" are detected via the 84+50n size equation.
- Triangle count is capped at 8M for thumbnail rendering.

## Verify

```
python test\test_render.py                          :: renders via the DLL C API
test\build_verify.cmd                               :: builds + runs the shell-pipeline check
build\verify_shell.exe <abs path>.stl out.bmp       :: IShellItemImageFactory (same path Explorer uses)
```
