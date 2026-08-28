// Flat C API so the config helper (C#) and the test harness can reuse the
// exact same renderer + the DLL's registry config plumbing.
#include <windows.h>
#include <vector>
#include <cstdio>
#include "renderer.h"
#include "config.h"

#pragma pack(push, 1)
struct BmpHeaders {
    // BITMAPFILEHEADER (written field-by-field due to packing) + V4 header for alpha
    uint16_t bfType; uint32_t bfSize; uint16_t r1, r2; uint32_t bfOffBits;
    BITMAPV4HEADER v4;
};
#pragma pack(pop)

static bool writeBmp32(const wchar_t* path, const std::vector<uint32_t>& px, int size) {
    BmpHeaders h = {};
    h.bfType = 0x4D42;
    h.bfOffBits = sizeof(BmpHeaders);
    h.bfSize = h.bfOffBits + (uint32_t)(px.size() * 4);
    h.v4.bV4Size = sizeof(BITMAPV4HEADER);
    h.v4.bV4Width = size;
    h.v4.bV4Height = -size; // top-down
    h.v4.bV4Planes = 1;
    h.v4.bV4BitCount = 32;
    h.v4.bV4V4Compression = BI_BITFIELDS;
    h.v4.bV4RedMask = 0x00FF0000; h.v4.bV4GreenMask = 0x0000FF00;
    h.v4.bV4BlueMask = 0x000000FF; h.v4.bV4AlphaMask = 0xFF000000;
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"wb") || !f) return false;
    fwrite(&h, sizeof(h), 1, f);
    fwrite(px.data(), 4, px.size(), f);
    fclose(f);
    return true;
}

// Renders an STL file to a 32-bit BMP.
// cfgOverride: 0 = use registry config; else pointer to 9 ints
//   [yaw, pitch, modelColor, bgColor, bgTransparent, ambient, diffuse, lightYaw, lightPitch]
// Returns 0 on success, negative error code otherwise.
extern "C" __declspec(dllexport)
int __stdcall StlRenderToFile(const wchar_t* stlPath, const wchar_t* bmpPath,
                              int size, const int* cfgOverride)
{
    if (!stlPath || !bmpPath || size < 8 || size > 2048) return -1;

    HANDLE hf = CreateFileW(stlPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return -2;
    LARGE_INTEGER sz;
    GetFileSizeEx(hf, &sz);
    if (sz.QuadPart == 0 || sz.QuadPart > 512ll * 1024 * 1024) { CloseHandle(hf); return -3; }
    std::vector<uint8_t> buf((size_t)sz.QuadPart);
    DWORD got = 0; size_t off = 0;
    while (off < buf.size()) {
        DWORD want = (DWORD)std::min<size_t>(buf.size() - off, 1 << 20);
        if (!ReadFile(hf, buf.data() + off, want, &got, nullptr) || got == 0) break;
        off += got;
    }
    CloseHandle(hf);
    if (off != buf.size()) return -4;

    std::vector<float> tris;
    if (!stlthumb::LoadStl(buf.data(), buf.size(), tris)) return -5;

    stlthumb::RenderConfig cfg;
    if (cfgOverride) {
        cfg.yawDeg = cfgOverride[0]; cfg.pitchDeg = cfgOverride[1];
        cfg.modelColor = (uint32_t)cfgOverride[2]; cfg.bgColor = (uint32_t)cfgOverride[3];
        cfg.bgTransparent = cfgOverride[4] != 0;
        cfg.ambientPct = cfgOverride[5]; cfg.diffusePct = cfgOverride[6];
        cfg.lightYawDeg = cfgOverride[7]; cfg.lightPitchDeg = cfgOverride[8];
    } else {
        cfg = stlthumb::LoadConfigFromRegistry();
    }

    std::vector<uint32_t> px;
    if (!stlthumb::RenderStl(tris, size, cfg, px)) return -6;
    if (!writeBmp32(bmpPath, px, size)) return -7;
    return 0;
}
