// Standalone regression test for LoadObj — exercises the parser directly
// (no COM/shell surrogate), so it always terminates and catches the
// parser-hang class of bug that a malformed or Rhino-exported OBJ can trigger.
//
// usage: test_obj <file.obj> [expected_triangles]
//   With expected_triangles, asserts the count and returns 0 (PASS) / 1 (FAIL).
//   Without it, just reports and returns 0 if any triangles were produced.
#include "../src/renderer.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <fstream>

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: test_obj <file.obj> [expected_triangles]\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    if (!f) { printf("cannot open %s\n", argv[1]); return 2; }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    std::vector<float> tris;
    bool ok = stlthumb::LoadObj(buf.data(), buf.size(), tris);
    size_t ntri = tris.size() / 9;
    printf("%s: read %zu bytes, LoadObj ok=%d, triangles=%zu\n",
           argv[1], buf.size(), (int)ok, ntri);

    if (argc >= 3) {
        size_t expected = (size_t)strtoull(argv[2], nullptr, 10);
        if (ntri == expected) {
            printf("  PASS (expected %zu)\n", expected);
            return 0;
        }
        printf("  FAIL: expected %zu triangles, got %zu\n", expected, ntri);
        return 1;
    }
    return ok ? 0 : 1;
}
