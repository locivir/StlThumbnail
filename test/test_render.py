import math, struct, ctypes, os, sys

os.makedirs(r"D:\StlThumbnail\test", exist_ok=True)

# ---- generate a binary STL torus ----
def torus_tris(R=30, r=12, nu=64, nv=32):
    def pt(u, v):
        cu, su = math.cos(u), math.sin(u)
        cv, sv = math.cos(v), math.sin(v)
        return ((R + r*cv)*cu, (R + r*cv)*su, r*sv)
    tris = []
    for i in range(nu):
        for j in range(nv):
            u0, u1 = 2*math.pi*i/nu, 2*math.pi*(i+1)/nu
            v0, v1 = 2*math.pi*j/nv, 2*math.pi*(j+1)/nv
            a, b, c, d = pt(u0,v0), pt(u1,v0), pt(u1,v1), pt(u0,v1)
            tris.append((a,b,c)); tris.append((a,c,d))
    return tris

tris = torus_tris()
stl = r"D:\StlThumbnail\test\torus.stl"
with open(stl, "wb") as f:
    f.write(b"\x00"*80)
    f.write(struct.pack("<I", len(tris)))
    for t in tris:
        f.write(struct.pack("<3f", 0,0,0))
        for v in t: f.write(struct.pack("<3f", *v))
        f.write(struct.pack("<H", 0))
print("STL written:", len(tris), "tris,", os.path.getsize(stl), "bytes")

# also an ASCII STL pyramid to test the ASCII parser
asc = r"D:\StlThumbnail\test\pyramid.stl"
pts = [(0,0,20),(-10,-10,0),(10,-10,0),(10,10,0),(-10,10,0)]
faces = [(0,1,2),(0,2,3),(0,3,4),(0,4,1),(1,3,2),(1,4,3)]
with open(asc, "w") as f:
    f.write("solid pyramid\n")
    for fa in faces:
        f.write(" facet normal 0 0 0\n  outer loop\n")
        for idx in fa:
            f.write("   vertex %g %g %g\n" % pts[idx])
        f.write("  endloop\n endfacet\n")
    f.write("endsolid pyramid\n")

# ---- render via the DLL ----
dll = ctypes.WinDLL(r"D:\StlThumbnail\build\StlThumbnail.dll")
fn = dll.StlRenderToFile
fn.restype = ctypes.c_int
fn.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]

# default config (registry/defaults), and one with overrides
rc = fn(stl, r"D:\StlThumbnail\test\torus_default.bmp", 512, None)
print("torus default render rc =", rc)

Cfg = (ctypes.c_int * 9)(60, 35, 0x4090E0, 0x202030, 0, 25, 90, 40, 60)
rc2 = fn(stl, r"D:\StlThumbnail\test\torus_blue.bmp", 512, Cfg)
print("torus override render rc =", rc2)

rc3 = fn(asc, r"D:\StlThumbnail\test\pyramid.bmp", 512, None)
print("pyramid (ascii) render rc =", rc3)

# ---- convert BMPs to PNG for viewing ----
try:
    from PIL import Image
    for n in ["torus_default", "torus_blue", "pyramid"]:
        p = rf"D:\StlThumbnail\test\{n}.bmp"
        Image.open(p).convert("RGBA").save(rf"D:\StlThumbnail\test\{n}.png")
    print("PNGs written")
except ImportError:
    print("PIL not available")

sys.exit(0 if (rc == 0 and rc2 == 0 and rc3 == 0) else 1)
