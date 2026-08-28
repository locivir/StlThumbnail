// StlThumbnail — Windows Explorer thumbnail provider for .stl files.
// Implements IInitializeWithStream + IThumbnailProvider in a classic COM DLL.
#include <windows.h>
#include <shlwapi.h>
#include <thumbcache.h>
#include <shlobj.h>
#include <new>
#include <vector>
#include "renderer.h"
#include "config.h"

#pragma comment(lib, "shlwapi.lib")

// {5E9BBB1E-9B37-4E22-A0F0-3D8B62D3C10A}
static const CLSID CLSID_StlThumbProvider =
{ 0x5e9bbb1e, 0x9b37, 0x4e22, { 0xa0, 0xf0, 0x3d, 0x8b, 0x62, 0xd3, 0xc1, 0x0a } };

static LONG g_cDllRef = 0;
static HINSTANCE g_hInst = nullptr;

// ---------------------------------------------------------------- provider

class StlThumbProvider : public IThumbnailProvider, public IInitializeWithStream
{
public:
    StlThumbProvider() : m_cRef(1), m_stream(nullptr) { InterlockedIncrement(&g_cDllRef); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        static const QITAB qit[] = {
            QITABENT(StlThumbProvider, IThumbnailProvider),
            QITABENT(StlThumbProvider, IInitializeWithStream),
            { 0 },
        };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG c = InterlockedDecrement(&m_cRef);
        if (!c) delete this;
        return c;
    }

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pStream, DWORD) override {
        if (m_stream) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        m_stream = pStream;
        m_stream->AddRef();
        return S_OK;
    }

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override {
        *phbmp = nullptr;
        *pdwAlpha = WTSAT_ARGB;
        if (!m_stream) return E_UNEXPECTED;
        if (cx < 1) cx = 96;
        if (cx > 1024) cx = 1024;

        // Read the whole stream (STL files are self-contained).
        STATSTG st = {};
        if (FAILED(m_stream->Stat(&st, STATFLAG_NONAME))) return E_FAIL;
        if (st.cbSize.QuadPart == 0 || st.cbSize.QuadPart > 512ull * 1024 * 1024) return E_FAIL;
        size_t len = (size_t)st.cbSize.QuadPart;
        std::vector<uint8_t> buf;
        try { buf.resize(len); } catch (...) { return E_OUTOFMEMORY; }
        LARGE_INTEGER zero = {};
        m_stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        size_t off = 0;
        while (off < len) {
            ULONG want = (ULONG)((len - off > 1 << 20) ? 1 << 20 : len - off);
            ULONG got = 0;
            HRESULT hr = m_stream->Read(buf.data() + off, want, &got);
            if (FAILED(hr) || got == 0) break;
            off += got;
        }
        if (off != len) return E_FAIL;

        std::vector<float> tris;
        // Try STL first (binary or ASCII), then OBJ.
        if (!stlthumb::LoadStl(buf.data(), len, tris) && !stlthumb::LoadObj(buf.data(), len, tris)) return E_FAIL;
        buf.clear(); buf.shrink_to_fit();

        stlthumb::RenderConfig cfg = stlthumb::LoadConfigFromRegistry();
        std::vector<uint32_t> pixels;
        if (!stlthumb::RenderStl(tris, (int)cx, cfg, pixels)) return E_FAIL;

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = (LONG)cx;
        bmi.bmiHeader.biHeight = -(LONG)cx; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hbmp || !bits) { if (hbmp) DeleteObject(hbmp); return E_OUTOFMEMORY; }
        memcpy(bits, pixels.data(), (size_t)cx * cx * 4);
        *phbmp = hbmp;
        *pdwAlpha = WTSAT_ARGB;
        return S_OK;
    }

private:
    ~StlThumbProvider() {
        if (m_stream) m_stream->Release();
        InterlockedDecrement(&g_cDllRef);
    }
    LONG m_cRef;
    IStream* m_stream;
};

// ---------------------------------------------------------------- factory

class ClassFactory : public IClassFactory
{
public:
    ClassFactory() : m_cRef(1) { InterlockedIncrement(&g_cDllRef); }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        static const QITAB qit[] = { QITABENT(ClassFactory, IClassFactory), { 0 } };
        return QISearch(this, qit, riid, ppv);
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_cRef); }
    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG c = InterlockedDecrement(&m_cRef);
        if (!c) delete this;
        return c;
    }
    IFACEMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv) override {
        *ppv = nullptr;
        if (pOuter) return CLASS_E_NOAGGREGATION;
        StlThumbProvider* p = new (std::nothrow) StlThumbProvider();
        if (!p) return E_OUTOFMEMORY;
        HRESULT hr = p->QueryInterface(riid, ppv);
        p->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) InterlockedIncrement(&g_cDllRef);
        else InterlockedDecrement(&g_cDllRef);
        return S_OK;
    }
private:
    ~ClassFactory() { InterlockedDecrement(&g_cDllRef); }
    LONG m_cRef;
};

// ---------------------------------------------------------------- exports

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, CLSID_StlThumbProvider)) return CLASS_E_CLASSNOTAVAILABLE;
    ClassFactory* f = new (std::nothrow) ClassFactory();
    if (!f) return E_OUTOFMEMORY;
    HRESULT hr = f->QueryInterface(riid, ppv);
    f->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return g_cDllRef > 0 ? S_FALSE : S_OK;
}

// ------------------------------------------------ self registration

static HRESULT SetRegValue(HKEY root, const wchar_t* sub, const wchar_t* name, const wchar_t* val) {
    HKEY hKey;
    LONG r = RegCreateKeyExW(root, sub, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (r != ERROR_SUCCESS) return HRESULT_FROM_WIN32(r);
    r = RegSetValueExW(hKey, name, 0, REG_SZ, (const BYTE*)val, (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(r);
}

// Per-user registration (HKCU) — no admin needed.
STDAPI DllRegisterServer() {
    wchar_t module[MAX_PATH];
    if (!GetModuleFileNameW(g_hInst, module, MAX_PATH)) return E_FAIL;

    const wchar_t* clsidStr = L"{5E9BBB1E-9B37-4E22-A0F0-3D8B62D3C10A}";
    wchar_t key[256];

    swprintf_s(key, L"Software\\Classes\\CLSID\\%s", clsidStr);
    HRESULT hr = SetRegValue(HKEY_CURRENT_USER, key, nullptr, L"STL Thumbnail Provider");
    if (FAILED(hr)) return hr;

    swprintf_s(key, L"Software\\Classes\\CLSID\\%s\\InprocServer32", clsidStr);
    hr = SetRegValue(HKEY_CURRENT_USER, key, nullptr, module);
    if (FAILED(hr)) return hr;
    hr = SetRegValue(HKEY_CURRENT_USER, key, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;

    // Bind to .stl and .obj via the IThumbnailProvider shellex interface GUID.
    hr = SetRegValue(HKEY_CURRENT_USER,
        L"Software\\Classes\\.stl\\ShellEx\\{E357FCCD-A995-4576-B01F-234630154E96}",
        nullptr, clsidStr);
    if (FAILED(hr)) return hr;

    hr = SetRegValue(HKEY_CURRENT_USER,
        L"Software\\Classes\\.obj\\ShellEx\\{E357FCCD-A995-4576-B01F-234630154E96}",
        nullptr, clsidStr);
    if (FAILED(hr)) return hr;

    // Explorer runs thumbnail handlers in a surrogate; treat-as-image hint:
    SetRegValue(HKEY_CURRENT_USER, L"Software\\Classes\\.stl", L"PerceivedType", L"Document");
    SetRegValue(HKEY_CURRENT_USER, L"Software\\Classes\\.obj", L"PerceivedType", L"Document");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

STDAPI DllUnregisterServer() {
    const wchar_t* clsidStr = L"{5E9BBB1E-9B37-4E22-A0F0-3D8B62D3C10A}";
    wchar_t key[256];
    swprintf_s(key, L"Software\\Classes\\CLSID\\%s", clsidStr);
    RegDeleteTreeW(HKEY_CURRENT_USER, key);
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.stl\\ShellEx\\{E357FCCD-A995-4576-B01F-234630154E96}");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\.obj\\ShellEx\\{E357FCCD-A995-4576-B01F-234630154E96}");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInst = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}
