// Standalone test for LoadObj — verifies the Rhino OBJ face format parses
// without hanging and produces triangles.
#include "../src/renderer.h"
#include <cstdio>
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: test_obj <file.obj>\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { printf("cannot open %s\n", argv[1]); return 2; }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    printf("read %zu bytes\n", buf.size());
    std::vector<float> tris;
    bool ok = stlthumb::LoadObj(buf.data(), buf.size(), tris);
    printf("LoadObj ok=%d, triangles=%zu\n", (int)ok, tris.size() / 9);
    return ok ? 0 : 1;
}
