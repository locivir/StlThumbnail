#pragma once
#include <windows.h>
#include "renderer.h"

// Registry location shared between the COM handler and the config helper app.
#define STLTHUMB_REG_KEY  L"Software\\StlThumbnail"

namespace stlthumb {

// Loads RenderConfig from HKCU\Software\StlThumbnail (falls back to defaults).
inline RenderConfig LoadConfigFromRegistry() {
    RenderConfig cfg;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STLTHUMB_REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return cfg;
    auto rd = [&](const wchar_t* name, int def) -> int {
        DWORD val = 0, sz = sizeof(val), type = 0;
        if (RegQueryValueExW(hKey, name, nullptr, &type, (LPBYTE)&val, &sz) == ERROR_SUCCESS && type == REG_DWORD)
            return (int)val;
        return def;
    };
    cfg.yawDeg        = rd(L"Yaw", cfg.yawDeg);
    cfg.pitchDeg      = rd(L"Pitch", cfg.pitchDeg);
    cfg.modelColor    = (uint32_t)rd(L"ModelColor", (int)cfg.modelColor);
    cfg.bgColor       = (uint32_t)rd(L"BgColor", (int)cfg.bgColor);
    cfg.bgTransparent = rd(L"BgTransparent", cfg.bgTransparent ? 1 : 0) != 0;
    cfg.ambientPct    = rd(L"Ambient", cfg.ambientPct);
    cfg.diffusePct    = rd(L"Diffuse", cfg.diffusePct);
    cfg.lightYawDeg   = rd(L"LightYaw", cfg.lightYawDeg);
    cfg.lightPitchDeg = rd(L"LightPitch", cfg.lightPitchDeg);
    RegCloseKey(hKey);
    return cfg;
}

} // namespace stlthumb
