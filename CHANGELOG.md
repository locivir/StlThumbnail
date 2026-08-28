# Changelog

All notable changes to StlThumbnail are documented here.

## [Unreleased]

### Added
- **Wavefront `.obj` thumbnail support.** The handler now renders `.obj`
  files in Windows Explorer alongside `.stl`. A new `LoadObj` parser
  (`src/renderer.cpp`) reads `v x y z` vertices and `f ...` faces, feeding the
  same software rasterizer used for STL.
  - Handles Rhino's `f v/vt/vn` slash-separated face indices (texture/normal
    parts ignored).
  - Triangulates quadrilateral faces (splits each quad into two triangles).
  - Supports negative (end-relative) vertex indices.
- Per-user registry binding for `.obj` (`DllRegisterServer`), including
  `PerceivedType=Document`, with matching cleanup in `DllUnregisterServer`.
- `test/test_obj.cpp` — a standalone regression harness that loads an OBJ file
  through `LoadObj` and reports triangle count, proving the parser terminates.
- Rhino 8 test fixtures: `test/test_stl.obj` (mesh export) and
  `test/test_nurbs.obj` (NURBS surface export, no mesh faces).

### Fixed
- **Explorer thumbnail generation hung for every file type.** Two infinite
  loops in the initial OBJ parser froze the COM thumbnail surrogate
  (`dllhost.exe`) at 100% CPU. Because that surrogate holds the shared
  thumbnail class object, it jammed *all* thumbnail requests — STL included —
  not just OBJ.
  - Face-index tokeniser stalled on the `/` in `v/vt/vn` and never advanced.
  - The line loop left the cursor parked on the `\r`/`\n` terminator, so it
    never progressed to the next line.
- Out-of-bounds read on malformed faces: face vertex indices are now validated
  against the vertex count before dereferencing, so a bad OBJ can no longer
  crash the shell host. NURBS-only exports (no faces) now fail cleanly instead
  of rendering nothing ambiguously.
- Off-by-one in negative OBJ index resolution.

### Notes
- OBJ cannot represent NURBS surfaces as a mesh; a NURBS-only Rhino export
  contains no `f` faces and produces no thumbnail by design. Export as a mesh
  from Rhino to get a preview.
