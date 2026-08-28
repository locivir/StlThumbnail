// Verifies the registered thumbnail handler through the real shell path:
// IShellItemImageFactory::GetImage(SIIGBF_THUMBNAILONLY) — same machinery
// Explorer uses. Saves result as BMP. usage: verify_shell.exe <file> <out.bmp>
#include <windows.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <cstdio>
#include <vector>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) { wprintf(L"usage: verify_shell <file.stl> <out.bmp>\n"); return 2; }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellItemImageFactory* fac = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(argv[1], nullptr, IID_PPV_ARGS(&fac));
    if (FAILED(hr)) { wprintf(L"SHCreateItem failed 0x%08X\n", hr); return 1; }
    HBITMAP hbmp = nullptr;
    SIZE s = { 256, 256 };
    hr = fac->GetImage(s, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &hbmp);
    fac->Release();
    if (FAILED(hr) || !hbmp) { wprintf(L"GetImage failed 0x%08X\n", hr); CoUninitialize(); return 1; }

    BITMAP bm;
    GetObject(hbmp, sizeof(bm), &bm);
    wprintf(L"thumbnail OK: %dx%d %dbpp\n", bm.bmWidth, bm.bmHeight, bm.bmBitsPixel);

    // save
    BITMAPINFOHEADER bi = { sizeof(bi), bm.bmWidth, -bm.bmHeight, 1, 32, BI_RGB };
    std::vector<uint32_t> px((size_t)bm.bmWidth * bm.bmHeight);
    HDC dc = GetDC(nullptr);
    GetDIBits(dc, hbmp, 0, bm.bmHeight, px.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    BITMAPFILEHEADER fh = { 0x4D42, (DWORD)(sizeof(fh) + sizeof(bi) + px.size() * 4), 0, 0, sizeof(fh) + sizeof(bi) };
    FILE* f = nullptr;
    _wfopen_s(&f, argv[2], L"wb");
    if (f) {
        fwrite(&fh, sizeof(fh), 1, f); fwrite(&bi, sizeof(bi), 1, f);
        fwrite(px.data(), 4, px.size(), f); fclose(f);
    }
    DeleteObject(hbmp);
    CoUninitialize();
    return 0;
}
