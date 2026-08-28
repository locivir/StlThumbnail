#include "renderer.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <limits>

namespace stlthumb {

// ---------------------------------------------------------------- STL parsing

static bool isAsciiStl(const uint8_t* data, size_t len) {
    // Binary STL: 80-byte header + 4-byte count + 50*count bytes.
    // A file starting with "solid" is *usually* ASCII, but some binary files
    // also start with "solid", so verify the binary size equation too.
    if (len < 15) return false;
    size_t i = 0;
    while (i < len && (data[i] == ' ' || data[i] == '\t' || data[i] == '\r' || data[i] == '\n')) i++;
    if (i + 5 > len || memcmp(data + i, "solid", 5) != 0) return false;
    if (len >= 84) {
        uint32_t n;
        memcpy(&n, data + 80, 4);
        if ((uint64_t)84 + (uint64_t)n * 50 == len) return false; // consistent binary
    }
    // Require the word "facet" somewhere early to call it ASCII.
    size_t scan = std::min<size_t>(len, 4096);
    for (size_t j = i; j + 5 <= scan; ++j)
        if (memcmp(data + j, "facet", 5) == 0) return true;
    // Tiny valid ASCII file with zero facets ("solid x\nendsolid x")
    for (size_t j = i; j + 8 <= scan; ++j)
        if (memcmp(data + j, "endsolid", 8) == 0) return true;
    return false;
}

static bool loadBinary(const uint8_t* data, size_t len, std::vector<float>& tris) {
    if (len < 84) return false;
    uint32_t n;
    memcpy(&n, data + 80, 4);
    uint64_t need = 84ull + 50ull * n;
    if (n == 0 || need > len) return false;
    // Cap absurd files (thumbnails don't need >8M triangles)
    const uint32_t cap = 8u * 1000 * 1000;
    uint32_t use = std::min(n, cap);
    tris.reserve((size_t)use * 9);
    const uint8_t* p = data + 84;
    for (uint32_t i = 0; i < use; ++i, p += 50) {
        float v[9];
        memcpy(v, p + 12, 36); // skip 12-byte normal, read 3 vertices
        for (int k = 0; k < 9; ++k) tris.push_back(v[k]);
    }
    return !tris.empty();
}

static bool loadAscii(const uint8_t* data, size_t len, std::vector<float>& tris) {
    const char* p = (const char*)data;
    const char* end = p + len;
    float v[9];
    int have = 0;
    while (p < end) {
        // find "vertex"
        const char* q = p;
        while (q + 6 <= end && memcmp(q, "vertex", 6) != 0) ++q;
        if (q + 6 > end) break;
        q += 6;
        char* after = nullptr;
        for (int k = 0; k < 3; ++k) {
            v[have * 3 + k] = strtof(q, &after);
            if (after == q) return !tris.empty();
            q = after;
        }
        have++;
        if (have == 3) {
            for (int k = 0; k < 9; ++k) tris.push_back(v[k]);
            have = 0;
        }
        p = q;
    }
    return !tris.empty();
}

bool LoadStl(const uint8_t* data, size_t len, std::vector<float>& tris) {
    tris.clear();
    if (!data || len < 15) return false;
    if (isAsciiStl(data, len)) return loadAscii(data, len, tris);
    return loadBinary(data, len, tris);
}

// ---------------------------------------------------------------- math

struct Vec3 { float x, y, z; };
static inline Vec3 sub(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline Vec3 cross(Vec3 a, Vec3 b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 norm(Vec3 a) {
    float l = sqrtf(dot(a, a));
    if (l < 1e-20f) return { 0, 0, 1 };
    return { a.x / l, a.y / l, a.z / l };
}

// ---------------------------------------------------------------- rendering

bool RenderStl(const std::vector<float>& tris, int size, const RenderConfig& cfg,
               std::vector<uint32_t>& outBGRA) {
    size_t ntri = tris.size() / 9;
    if (ntri == 0 || size <= 0) return false;

    const float DEG = 3.14159265358979f / 180.0f;

    // Camera rotation: model Z is "up". Yaw around Z, then pitch (tilt toward viewer).
    float cy = cosf(cfg.yawDeg * DEG),  sy = sinf(cfg.yawDeg * DEG);
    float cp = cosf(cfg.pitchDeg * DEG), sp = sinf(cfg.pitchDeg * DEG);
    // world -> view:  v' = Rx(pitch) * Rz(yaw) * v   (then screen x = v'.x, y = -v'.z(view up))
    auto toView = [&](Vec3 v) -> Vec3 {
        float x1 = cy * v.x + sy * v.y;
        float y1 = -sy * v.x + cy * v.y;
        float z1 = v.z;
        // pitch: rotate around view X axis; view Y goes into screen depth
        float y2 = cp * y1 + sp * z1;   // depth (larger = farther)
        float z2 = -sp * y1 + cp * z1;  // up on screen
        return { x1, y2, z2 };
    };

    // Transform vertices, track bounds in view space.
    std::vector<Vec3> vv((size_t)ntri * 3);
    float minx = FLT_MAX, maxx = -FLT_MAX, minz = FLT_MAX, maxz = -FLT_MAX;
    float miny = FLT_MAX, maxy = -FLT_MAX;
    for (size_t i = 0; i < ntri * 3; ++i) {
        Vec3 w = { tris[i * 3], tris[i * 3 + 1], tris[i * 3 + 2] };
        Vec3 v = toView(w);
        vv[i] = v;
        minx = std::min(minx, v.x); maxx = std::max(maxx, v.x);
        minz = std::min(minz, v.z); maxz = std::max(maxz, v.z);
        miny = std::min(miny, v.y); maxy = std::max(maxy, v.y);
    }
    float w = maxx - minx, h = maxz - minz;
    if (w <= 0 && h <= 0) return false;
    float margin = size * 0.06f;
    float scale = (size - 2 * margin) / std::max(std::max(w, h), 1e-9f);
    float cxm = (minx + maxx) * 0.5f, czm = (minz + maxz) * 0.5f;
    float half = size * 0.5f;

    // Light direction (points FROM light TOWARD scene), in view space? No —
    // keep light fixed relative to the camera so results are predictable.
    float lcy = cosf(cfg.lightYawDeg * DEG),  lsy = sinf(cfg.lightYawDeg * DEG);
    float lcp = cosf(cfg.lightPitchDeg * DEG), lsp = sinf(cfg.lightPitchDeg * DEG);
    // Camera-space light: yaw around screen-up, pitch up from view axis.
    Vec3 L = norm(Vec3{ lsy * lcp, -lcp * lcy, lsp }); // toward light

    float ambient = std::clamp(cfg.ambientPct, 0, 100) / 100.0f;
    float diffuse = std::clamp(cfg.diffusePct, 0, 100) / 100.0f;

    float mr = ((cfg.modelColor >> 16) & 0xFF) / 255.0f;
    float mg = ((cfg.modelColor >> 8) & 0xFF) / 255.0f;
    float mb = (cfg.modelColor & 0xFF) / 255.0f;

    // Buffers
    std::vector<float> zbuf((size_t)size * size, FLT_MAX);
    outBGRA.assign((size_t)size * size, 0);
    uint32_t bg = 0;
    if (!cfg.bgTransparent) {
        bg = 0xFF000000u | ((cfg.bgColor >> 16 & 0xFF) << 16) | ((cfg.bgColor >> 8 & 0xFF) << 8) | (cfg.bgColor & 0xFF);
        std::fill(outBGRA.begin(), outBGRA.end(), bg);
    }

    // Rasterize
    for (size_t t = 0; t < ntri; ++t) {
        Vec3 a = vv[t * 3], b = vv[t * 3 + 1], c = vv[t * 3 + 2];
        // screen coords
        float ax = (a.x - cxm) * scale + half, ay = half - (a.z - czm) * scale;
        float bx = (b.x - cxm) * scale + half, by = half - (b.z - czm) * scale;
        float cx = (c.x - cxm) * scale + half, cy2 = half - (c.z - czm) * scale;

        // Face normal in view space (for shading). Recompute from geometry;
        // stored STL normals are often garbage.
        Vec3 n = norm(cross(sub(b, a), sub(c, a)));
        // Make normal face the camera (viewer looks along +y into the screen)
        if (n.y > 0) { n.x = -n.x; n.y = -n.y; n.z = -n.z; }
        float lit = ambient + diffuse * std::max(0.0f, dot(n, L));
        lit = std::min(lit, 1.0f);
        uint8_t rr = (uint8_t)(std::min(mr * lit, 1.0f) * 255.0f + 0.5f);
        uint8_t gg = (uint8_t)(std::min(mg * lit, 1.0f) * 255.0f + 0.5f);
        uint8_t bb2 = (uint8_t)(std::min(mb * lit, 1.0f) * 255.0f + 0.5f);
        uint32_t px = 0xFF000000u | (rr << 16) | (gg << 8) | bb2;

        int x0 = std::max(0, (int)floorf(std::min({ ax, bx, cx })));
        int x1 = std::min(size - 1, (int)ceilf(std::max({ ax, bx, cx })));
        int y0 = std::max(0, (int)floorf(std::min({ ay, by, cy2 })));
        int y1 = std::min(size - 1, (int)ceilf(std::max({ ay, by, cy2 })));
        if (x0 > x1 || y0 > y1) continue;

        float d = (bx - ax) * (cy2 - ay) - (by - ay) * (cx - ax);
        if (fabsf(d) < 1e-9f) continue;
        float invd = 1.0f / d;

        for (int y = y0; y <= y1; ++y) {
            float py = y + 0.5f;
            for (int x = x0; x <= x1; ++x) {
                float pxx = x + 0.5f;
                float w0 = ((bx - ax) * (py - ay) - (by - ay) * (pxx - ax)) * invd;
                float w1 = ((cx - bx) * (py - by) - (cy2 - by) * (pxx - bx)) * invd;
                // barycentric via sub-areas
                float l2 = w1;                       // weight of a
                float l0 = w0;                       // weight of c
                float l1 = 1.0f - l0 - l2;           // weight of b
                if (l0 < 0 || l1 < 0 || l2 < 0) continue;
                float depth = a.y * l2 + b.y * l1 + c.y * l0;
                size_t idx = (size_t)y * size + x;
                if (depth < zbuf[idx]) {
                    zbuf[idx] = depth;
                    outBGRA[idx] = px;
                }
            }
        }
    }

    // 2x2-ish edge smoothing: cheap post-AA — blend pixels whose neighbors differ.
    // (Skipped: keep it simple & fast; Explorer thumbnails are small.)
    return true;
}

} // namespace stlthumb
