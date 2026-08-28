#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace stlthumb {

// Render configuration. Angles in degrees, percentages 0..100.
struct RenderConfig {
    int      yawDeg        = 30;    // camera azimuth around Z (up) axis
    int      pitchDeg      = 25;    // camera elevation
    uint32_t modelColor    = 0xD4AF37; // 0xRRGGBB (default: gold)
    uint32_t bgColor       = 0xFFFFFF; // 0xRRGGBB
    bool     bgTransparent = true;
    int      ambientPct    = 30;    // ambient light 0..100
    int      diffusePct    = 80;    // key light strength 0..100
    int      lightYawDeg   = -40;   // key light azimuth
    int      lightPitchDeg = 45;    // key light elevation
};

// Parses ASCII or binary STL from a memory buffer.
// On success fills `tris` with 9 floats per triangle (v0 v1 v2, xyz each).
bool LoadStl(const uint8_t* data, size_t len, std::vector<float>& tris);

// Parses OBJ (Wavefront) from a memory buffer.
// On success fills `tris` with 9 floats per triangle (v0 v1 v2, xyz each).
// Supports triangular and quadrilateral faces (quads are triangulated).
bool LoadObj(const uint8_t* data, size_t len, std::vector<float>& tris);

// Renders triangles into a size x size 32bpp BGRA buffer (premultiplied alpha,
// top-down row order). Returns false if there is nothing to render.
bool RenderStl(const std::vector<float>& tris, int size, const RenderConfig& cfg,
               std::vector<uint32_t>& outBGRA);

} // namespace stlthumb
