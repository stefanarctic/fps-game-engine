#!/usr/bin/env python3
"""
Generates every asset the Arena FPS needs, procedurally, with no third-party
dependencies (pure Python standard library):

  textures/   grid.png, concrete.png, crate.png, metal.png, hazard.png, drone_panels.png
  models/     crate.obj/.mtl, barrel.obj/.mtl, pillar.obj/.mtl, muzzleflash.obj/.mtl  (Wavefront OBJ)
              rifle.gltf + rifle.bin                                                (glTF 2.0, external buffer)
              drone.glb                                                             (glTF 2.0 binary, embedded PNG)
  materials/  *.mat (Forge material JSON)

Usage:  python tools/generate_assets.py [assets_dir]
"""
import json
import math
import os
import random
import struct
import sys
import zlib

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "assets"))


def path(*parts):
    p = os.path.join(ROOT, *parts)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    return p


# ---------------------------------------------------------------------------
# PNG writer
# ---------------------------------------------------------------------------
def png_bytes(width, height, pixels):
    """pixels: flat list of (r, g, b, a) tuples, row-major, top row first."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        row = pixels[y * width:(y + 1) * width]
        for r, g, b, a in row:
            raw += bytes((r, g, b, a))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")


def write_png(rel, width, height, pixels):
    with open(path(rel), "wb") as f:
        f.write(png_bytes(width, height, pixels))
    print("  texture  ", rel)


def clamp8(v):
    return 0 if v < 0 else (255 if v > 255 else int(v))


class TileNoise:
    """Tileable value noise with fractal octaves."""

    def __init__(self, seed, size=256):
        self.rng = random.Random(seed)
        self.size = size
        self.lattices = {}

    def lattice(self, n):
        if n not in self.lattices:
            self.lattices[n] = [self.rng.random() for _ in range(n * n)]
        return self.lattices[n]

    def value(self, x, y, cells):
        lat = self.lattice(cells)
        fx = x / self.size * cells
        fy = y / self.size * cells
        x0, y0 = int(math.floor(fx)), int(math.floor(fy))
        tx, ty = fx - x0, fy - y0
        tx = tx * tx * (3 - 2 * tx)
        ty = ty * ty * (3 - 2 * ty)
        x0 %= cells
        y0 %= cells
        x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
        a = lat[y0 * cells + x0]
        b = lat[y0 * cells + x1]
        c = lat[y1 * cells + x0]
        d = lat[y1 * cells + x1]
        return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty

    def fbm(self, x, y, base=4, octaves=4):
        total, amp, norm, cells = 0.0, 1.0, 0.0, base
        for _ in range(octaves):
            total += self.value(x, y, cells) * amp
            norm += amp
            amp *= 0.5
            cells *= 2
        return total / norm


def tex_grid():
    """Unity/ProBuilder-style prototype grid: 4x4 one-metre cells per texture."""
    n = 512
    noise = TileNoise(1, n)
    px = []
    for y in range(n):
        for x in range(n):
            v = 168 + (noise.fbm(x, y, 8, 3) - 0.5) * 14
            cx, cy = x % 128, y % 128
            if cx < 2 or cy < 2:
                v = 128
            elif cx == 64 or cy == 64:
                v -= 14
            if x < 4 or y < 4:
                v = 108
            px.append((clamp8(v), clamp8(v), clamp8(v + 4), 255))
    write_png("textures/grid.png", n, n, px)


def tex_concrete():
    n = 512
    noise = TileNoise(2, n)
    spots = TileNoise(3, n)
    px = []
    for y in range(n):
        for x in range(n):
            v = 150 + (noise.fbm(x, y, 8, 5) - 0.5) * 60
            s = spots.fbm(x, y, 16, 2)
            if s > 0.68:
                v -= (s - 0.68) * 160
            # Formwork seams every 256px
            if x % 256 < 2 or y % 256 < 2:
                v -= 35
            px.append((clamp8(v), clamp8(v * 0.99), clamp8(v * 0.96), 255))
    write_png("textures/concrete.png", n, n, px)


def tex_crate():
    n = 512
    grain = TileNoise(4, n)
    px = []
    border = 44
    for y in range(n):
        for x in range(n):
            g = grain.fbm(x * 0.25, y * 4.0, 8, 4)
            r, gg, b = 158 + (g - 0.5) * 70, 108 + (g - 0.5) * 50, 62 + (g - 0.5) * 30
            plank = (y - border) // 70
            if border <= y < n - border and (y - border) % 70 < 3:
                r, gg, b = r * 0.45, gg * 0.45, b * 0.45  # gaps between planks
            in_frame = x < border or x >= n - border or y < border or y >= n - border
            # Diagonal brace
            d = abs((x - border) - (y - border) * (n - 2 * border) / (n - 2 * border))
            in_brace = not in_frame and d < 26
            if in_frame or in_brace:
                r, gg, b = r * 0.78 + 10, gg * 0.72, b * 0.6
                edge = (x in (border - 1, n - border)) or (y in (border - 1, n - border)) or (in_brace and abs(d - 26) < 2.5)
                if edge:
                    r, gg, b = r * 0.5, gg * 0.5, b * 0.5
            else:
                shade = 0.92 + 0.08 * ((plank * 37) % 5) / 4.0
                r, gg, b = r * shade, gg * shade, b * shade
            # Nails in the frame corners
            for cx, cy in ((22, 22), (n - 22, 22), (22, n - 22), (n - 22, n - 22)):
                if (x - cx) ** 2 + (y - cy) ** 2 < 36:
                    r, gg, b = 70, 70, 75
            px.append((clamp8(r), clamp8(gg), clamp8(b), 255))
    write_png("textures/crate.png", n, n, px)


def tex_metal():
    n = 256
    noise = TileNoise(5, n)
    px = []
    for y in range(n):
        for x in range(n):
            streak = noise.value(x * 0.05, y, 64)
            v = 128 + (streak - 0.5) * 40 + (noise.fbm(x, y, 8, 3) - 0.5) * 20
            if x % 128 < 2 or y % 128 < 2:
                v -= 45
            for cx in (10, 118, 138, 246):
                for cy in (10, 118, 138, 246):
                    d = (x - cx) ** 2 + (y - cy) ** 2
                    if d < 16:
                        v = 175 - d * 4
            px.append((clamp8(v), clamp8(v + 2), clamp8(v + 6), 255))
    write_png("textures/metal.png", n, n, px)


def tex_hazard():
    n = 256
    noise = TileNoise(6, n)
    px = []
    for y in range(n):
        for x in range(n):
            stripe = ((x + y) // 32) % 2 == 0
            wear = noise.fbm(x, y, 16, 3)
            if stripe:
                r, g, b = 235, 180, 30
            else:
                r, g, b = 30, 30, 32
            if wear > 0.7:
                r, g, b = r * 0.7 + 30, g * 0.7 + 30, b * 0.7 + 30
            px.append((clamp8(r), clamp8(g), clamp8(b), 255))
    write_png("textures/hazard.png", n, n, px)


def tex_drone_panels():
    n = 128
    noise = TileNoise(7, n)
    px = []
    for y in range(n):
        for x in range(n):
            v = 205 + (noise.fbm(x, y, 8, 3) - 0.5) * 30
            if x % 32 < 1 or y % 43 < 1:
                v -= 90
            if (x // 32 + y // 43) % 3 == 0:
                v -= 25
            px.append((clamp8(v), clamp8(v), clamp8(v + 5), 255))
    data = png_bytes(n, n, px)
    with open(path("textures/drone_panels.png"), "wb") as f:
        f.write(data)
    print("  texture   textures/drone_panels.png")
    return data


# ---------------------------------------------------------------------------
# Geometry helpers (shared by OBJ and glTF writers)
# ---------------------------------------------------------------------------
class MeshData:
    def __init__(self):
        self.pos, self.nrm, self.uv, self.idx = [], [], [], []

    def vert(self, p, n, t):
        self.pos.append(tuple(p))
        self.nrm.append(tuple(n))
        self.uv.append(tuple(t))
        return len(self.pos) - 1

    def quad(self, a, b, c, d, n, uvs=((0, 0), (1, 0), (1, 1), (0, 1))):
        """Counter-clockwise when seen from the side the normal points to."""
        i = [self.vert(p, n, t) for p, t in zip((a, b, c, d), uvs)]
        self.idx += [i[0], i[1], i[2], i[0], i[2], i[3]]

    def box(self, center, size, uv_scale=None):
        cx, cy, cz = center
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        su = uv_scale

        def uvs(w, h):
            if su is None:
                return ((0, 0), (1, 0), (1, 1), (0, 1))
            return ((0, 0), (w * su, 0), (w * su, h * su), (0, h * su))

        X, Y, Z = size
        self.quad((cx - hx, cy - hy, cz + hz), (cx + hx, cy - hy, cz + hz), (cx + hx, cy + hy, cz + hz), (cx - hx, cy + hy, cz + hz), (0, 0, 1), uvs(X, Y))
        self.quad((cx + hx, cy - hy, cz - hz), (cx - hx, cy - hy, cz - hz), (cx - hx, cy + hy, cz - hz), (cx + hx, cy + hy, cz - hz), (0, 0, -1), uvs(X, Y))
        self.quad((cx + hx, cy - hy, cz + hz), (cx + hx, cy - hy, cz - hz), (cx + hx, cy + hy, cz - hz), (cx + hx, cy + hy, cz + hz), (1, 0, 0), uvs(Z, Y))
        self.quad((cx - hx, cy - hy, cz - hz), (cx - hx, cy - hy, cz + hz), (cx - hx, cy + hy, cz + hz), (cx - hx, cy + hy, cz - hz), (-1, 0, 0), uvs(Z, Y))
        self.quad((cx - hx, cy + hy, cz + hz), (cx + hx, cy + hy, cz + hz), (cx + hx, cy + hy, cz - hz), (cx - hx, cy + hy, cz - hz), (0, 1, 0), uvs(X, Z))
        self.quad((cx - hx, cy - hy, cz - hz), (cx + hx, cy - hy, cz - hz), (cx + hx, cy - hy, cz + hz), (cx - hx, cy - hy, cz + hz), (0, -1, 0), uvs(X, Z))

    def cylinder_y(self, center, radius, height, segments=24, caps=True, uv_repeat=1.0):
        cx, cy, cz = center
        y0, y1 = cy - height / 2, cy + height / 2
        ring = []
        for s in range(segments + 1):
            a = 2 * math.pi * s / segments
            n = (math.cos(a), 0.0, math.sin(a))
            u = s / segments * uv_repeat
            ring.append((self.vert((cx + n[0] * radius, y0, cz + n[2] * radius), n, (u, 0)),
                         self.vert((cx + n[0] * radius, y1, cz + n[2] * radius), n, (u, 1))))
        for s in range(segments):
            b0, t0 = ring[s]
            b1, t1 = ring[s + 1]
            self.idx += [b0, t0, t1, b0, t1, b1]
        if caps:
            for top in (True, False):
                y = y1 if top else y0
                n = (0, 1 if top else -1, 0)
                c = self.vert((cx, y, cz), n, (0.5, 0.5))
                pts = []
                for s in range(segments + 1):
                    a = 2 * math.pi * s / segments
                    pts.append(self.vert((cx + math.cos(a) * radius, y, cz + math.sin(a) * radius), n,
                                         (0.5 + math.cos(a) * 0.5, 0.5 + math.sin(a) * 0.5)))
                for s in range(segments):
                    if top:
                        self.idx += [c, pts[s + 1], pts[s]]
                    else:
                        self.idx += [c, pts[s], pts[s + 1]]

    def cylinder_z(self, center, radius, length, segments=16):
        """Cylinder along Z (gun barrels)."""
        cx, cy, cz = center
        z0, z1 = cz - length / 2, cz + length / 2
        ring = []
        for s in range(segments + 1):
            a = 2 * math.pi * s / segments
            n = (math.cos(a), math.sin(a), 0.0)
            ring.append((self.vert((cx + n[0] * radius, cy + n[1] * radius, z0), n, (s / segments, 0)),
                         self.vert((cx + n[0] * radius, cy + n[1] * radius, z1), n, (s / segments, 1))))
        for s in range(segments):
            a0, b0 = ring[s]
            a1, b1 = ring[s + 1]
            self.idx += [a0, a1, b1, a0, b1, b0]
        for front in (True, False):
            z = z0 if front else z1
            n = (0, 0, -1 if front else 1)
            c = self.vert((cx, cy, z), n, (0.5, 0.5))
            pts = [self.vert((cx + math.cos(2 * math.pi * s / segments) * radius, cy + math.sin(2 * math.pi * s / segments) * radius, z), n, (0.5, 0.5))
                   for s in range(segments + 1)]
            for s in range(segments):
                if front:
                    self.idx += [c, pts[s + 1], pts[s]]
                else:
                    self.idx += [c, pts[s], pts[s + 1]]

    def sphere(self, center, radius, segments=24, rings=14, scale=(1, 1, 1)):
        cx, cy, cz = center
        base = len(self.pos)
        for r in range(rings + 1):
            phi = math.pi * r / rings
            for s in range(segments + 1):
                th = 2 * math.pi * s / segments
                n = (math.sin(phi) * math.cos(th), math.cos(phi), math.sin(phi) * math.sin(th))
                p = (cx + n[0] * radius * scale[0], cy + n[1] * radius * scale[1], cz + n[2] * radius * scale[2])
                nn = (n[0] / scale[0], n[1] / scale[1], n[2] / scale[2])
                ln = math.sqrt(sum(c * c for c in nn)) or 1.0
                self.vert(p, (nn[0] / ln, nn[1] / ln, nn[2] / ln), (s / segments * 2.0, 1 - r / rings))
        for r in range(rings):
            for s in range(segments):
                a = base + r * (segments + 1) + s
                b = a + segments + 1
                self.idx += [a, a + 1, b, b, a + 1, b + 1]


# ---------------------------------------------------------------------------
# OBJ writer
# ---------------------------------------------------------------------------
def write_obj(name, groups, materials):
    """groups: list of (material_name, MeshData). materials: dict name -> mtl lines."""
    obj = path("models", name + ".obj")
    mtl = path("models", name + ".mtl")
    with open(mtl, "w") as f:
        f.write("# Forge Arena - generated by tools/generate_assets.py\n")
        for mname, lines in materials.items():
            f.write("\nnewmtl %s\n" % mname)
            for line in lines:
                f.write(line + "\n")
    with open(obj, "w") as f:
        f.write("# Forge Arena - generated by tools/generate_assets.py\n")
        f.write("mtllib %s.mtl\n" % name)
        offset = 0
        for gi, (mname, m) in enumerate(groups):
            f.write("o %s_%d\n" % (mname, gi))
            for p in m.pos:
                f.write("v %.5f %.5f %.5f\n" % p)
            for t in m.uv:
                f.write("vt %.5f %.5f\n" % t)
            for n in m.nrm:
                f.write("vn %.5f %.5f %.5f\n" % n)
            f.write("usemtl %s\n" % mname)
            for i in range(0, len(m.idx), 3):
                a, b, c = (m.idx[i] + 1 + offset, m.idx[i + 1] + 1 + offset, m.idx[i + 2] + 1 + offset)
                f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c))
            offset += len(m.pos)
    print("  model     models/%s.obj (+.mtl)" % name)


def model_crate():
    m = MeshData()
    m.box((0, 0.5, 0), (1, 1, 1))
    write_obj("crate", [("Crate", m)], {"Crate": ["Kd 1.0 1.0 1.0", "Ns 20", "map_Kd ../textures/crate.png"]})


def model_barrel():
    body = MeshData()
    body.cylinder_y((0, 0.6, 0), 0.4, 1.2, 28, caps=False)
    top = MeshData()  # lids
    for y, up in ((1.2, True), (0.0, False)):
        n = (0, 1 if up else -1, 0)
        c = top.vert((0, y, 0), n, (0.5, 0.5))
        pts = [top.vert((math.cos(2 * math.pi * s / 28) * 0.4, y, math.sin(2 * math.pi * s / 28) * 0.4), n,
                        (0.5 + math.cos(2 * math.pi * s / 28) * 0.5, 0.5 + math.sin(2 * math.pi * s / 28) * 0.5)) for s in range(29)]
        for s in range(28):
            top.idx += [c, pts[s + 1], pts[s]] if up else [c, pts[s], pts[s + 1]]
    rings = MeshData()
    for y in (0.08, 0.4, 0.8, 1.12):
        rings.cylinder_y((0, y, 0), 0.415, 0.06, 28, caps=True)
    write_obj("barrel", [("BarrelPaint", body), ("BarrelTop", top), ("BarrelRing", rings)], {
        "BarrelPaint": ["Kd 0.62 0.10 0.07", "Ns 60", "Pm 0.2"],
        "BarrelTop": ["Kd 0.35 0.35 0.36", "Ns 80", "Pm 0.6"],
        "BarrelRing": ["Kd 0.18 0.18 0.19", "Ns 120", "Pm 0.8"],
    })


def model_pillar():
    m = MeshData()
    m.box((0, 0.2, 0), (1.4, 0.4, 1.4), uv_scale=0.25)
    m.box((0, 2.25, 0), (1.0, 3.7, 1.0), uv_scale=0.25)
    m.box((0, 4.3, 0), (1.4, 0.4, 1.4), uv_scale=0.25)
    trim = MeshData()
    trim.box((0, 0.42, 0), (1.1, 0.06, 1.1))
    trim.box((0, 4.08, 0), (1.1, 0.06, 1.1))
    write_obj("pillar", [("Concrete", m), ("Trim", trim)], {
        "Concrete": ["Kd 0.85 0.85 0.85", "Ns 8", "map_Kd ../textures/concrete.png"],
        "Trim": ["Kd 0.95 0.45 0.10", "Ns 40"],
    })


def model_muzzleflash():
    m = MeshData()
    # Eight-point star facing +Z (towards the first-person camera)
    points = 8
    c = m.vert((0, 0, 0), (0, 0, 1), (0.5, 0.5))
    ring = []
    for i in range(points * 2 + 1):
        a = math.pi * i / points
        r = 0.13 if i % 2 == 0 else 0.045
        ring.append(m.vert((math.cos(a) * r, math.sin(a) * r, 0), (0, 0, 1), (0.5, 0.5)))
    for i in range(points * 2):
        m.idx += [c, ring[i], ring[i + 1]]
    # Two crossed spikes along -Z, double sided
    for (nx, ny) in ((1, 0), (0, 1)):
        w = 0.035
        a = (-w * ny, -w * nx, 0.0)
        b = (w * ny, w * nx, 0.0)
        tip = (0.0, 0.0, -0.32)
        n1 = (nx, ny, 0)
        i0 = m.vert(a, n1, (0, 0))
        i1 = m.vert(b, n1, (1, 0))
        i2 = m.vert(tip, n1, (0.5, 1))
        m.idx += [i0, i1, i2, i0, i2, i1]
    write_obj("muzzleflash", [("Flash", m)], {"Flash": ["Kd 1.0 0.8 0.4", "Ke 5.0 3.0 1.0"]})


# ---------------------------------------------------------------------------
# glTF writer
# ---------------------------------------------------------------------------
class GltfBuilder:
    def __init__(self):
        self.bin = bytearray()
        self.doc = {"asset": {"version": "2.0", "generator": "Forge generate_assets.py"},
                    "scene": 0, "scenes": [{"nodes": []}], "nodes": [], "meshes": [], "materials": [],
                    "buffers": [], "bufferViews": [], "accessors": []}

    def _align(self):
        while len(self.bin) % 4:
            self.bin.append(0)

    def _view(self, data, target=None):
        self._align()
        offset = len(self.bin)
        self.bin += data
        view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
        if target:
            view["target"] = target
        self.doc["bufferViews"].append(view)
        return len(self.doc["bufferViews"]) - 1

    def _accessor(self, data, ctype, count, type_, target, mn=None, mx=None):
        view = self._view(data, target)
        acc = {"bufferView": view, "componentType": ctype, "count": count, "type": type_}
        if mn is not None:
            acc["min"], acc["max"] = mn, mx
        self.doc["accessors"].append(acc)
        return len(self.doc["accessors"]) - 1

    def material(self, name, color, metallic=0.0, roughness=0.6, emissive=None, texture=None):
        m = {"name": name, "pbrMetallicRoughness": {"baseColorFactor": list(color) + [1.0],
                                                     "metallicFactor": metallic, "roughnessFactor": roughness}}
        if emissive:
            peak = max(emissive)
            if peak > 1.0:
                m["emissiveFactor"] = [c / peak for c in emissive]
                m.setdefault("extensions", {})["KHR_materials_emissive_strength"] = {"emissiveStrength": peak}
                self.doc.setdefault("extensionsUsed", [])
                if "KHR_materials_emissive_strength" not in self.doc["extensionsUsed"]:
                    self.doc["extensionsUsed"].append("KHR_materials_emissive_strength")
            else:
                m["emissiveFactor"] = list(emissive)
        if texture is not None:
            m["pbrMetallicRoughness"]["baseColorTexture"] = {"index": texture}
        self.doc["materials"].append(m)
        return len(self.doc["materials"]) - 1

    def embedded_png(self, png):
        view = self._view(png)
        self.doc.setdefault("images", []).append({"bufferView": view, "mimeType": "image/png"})
        self.doc.setdefault("samplers", []).append({"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497})
        self.doc.setdefault("textures", []).append({"sampler": len(self.doc["samplers"]) - 1, "source": len(self.doc["images"]) - 1})
        return len(self.doc["textures"]) - 1

    def mesh(self, name, data, material):
        pos = b"".join(struct.pack("<3f", *p) for p in data.pos)
        nrm = b"".join(struct.pack("<3f", *n) for n in data.nrm)
        # glTF texture coordinates have their origin at the top-left.
        uv = b"".join(struct.pack("<2f", t[0], 1.0 - t[1]) for t in data.uv)
        idx = b"".join(struct.pack("<H", i) for i in data.idx)
        mn = [min(p[i] for p in data.pos) for i in range(3)]
        mx = [max(p[i] for p in data.pos) for i in range(3)]
        prim = {"attributes": {"POSITION": self._accessor(pos, 5126, len(data.pos), "VEC3", 34962, mn, mx),
                               "NORMAL": self._accessor(nrm, 5126, len(data.nrm), "VEC3", 34962),
                               "TEXCOORD_0": self._accessor(uv, 5126, len(data.uv), "VEC2", 34962)},
                "indices": self._accessor(idx, 5123, len(data.idx), "SCALAR", 34963),
                "material": material}
        self.doc["meshes"].append({"name": name, "primitives": [prim]})
        return len(self.doc["meshes"]) - 1

    def node(self, name, mesh=None, translation=None, rotation=None, scale=None, parent=None):
        n = {"name": name}
        if mesh is not None:
            n["mesh"] = mesh
        if translation:
            n["translation"] = list(translation)
        if rotation:
            n["rotation"] = list(rotation)
        if scale:
            n["scale"] = list(scale)
        self.doc["nodes"].append(n)
        idx = len(self.doc["nodes"]) - 1
        if parent is None:
            self.doc["scenes"][0]["nodes"].append(idx)
        else:
            self.doc["nodes"][parent].setdefault("children", []).append(idx)
        return idx

    def save_gltf(self, rel_gltf, rel_bin):
        self._align()
        with open(path(rel_bin), "wb") as f:
            f.write(self.bin)
        self.doc["buffers"] = [{"uri": os.path.basename(rel_bin), "byteLength": len(self.bin)}]
        with open(path(rel_gltf), "w") as f:
            json.dump(self.doc, f, indent=1)
        print("  model    ", rel_gltf, "(+ %s)" % os.path.basename(rel_bin))

    def save_glb(self, rel):
        self._align()
        self.doc["buffers"] = [{"byteLength": len(self.bin)}]
        js = json.dumps(self.doc, separators=(",", ":")).encode("utf-8")
        while len(js) % 4:
            js += b" "
        total = 12 + 8 + len(js) + 8 + len(self.bin)
        with open(path(rel), "wb") as f:
            f.write(struct.pack("<4sII", b"glTF", 2, total))
            f.write(struct.pack("<I4s", len(js), b"JSON") + js)
            f.write(struct.pack("<I4s", len(self.bin), b"BIN\x00") + bytes(self.bin))
        print("  model    ", rel)


def quat_x(deg):
    a = math.radians(deg) / 2
    return (math.sin(a), 0.0, 0.0, math.cos(a))


def model_rifle():
    g = GltfBuilder()
    metal = g.material("GunMetal", (0.30, 0.31, 0.33), metallic=0.6, roughness=0.38)
    polymer = g.material("Polymer", (0.40, 0.34, 0.24), metallic=0.0, roughness=0.7)
    dark = g.material("Rubber", (0.12, 0.12, 0.12), metallic=0.0, roughness=0.8)
    dot = g.material("RedDot", (1.0, 0.1, 0.1), emissive=(6.0, 0.3, 0.2))

    def part(name, mat, build, translation=None, rotation=None, parent=None):
        m = MeshData()
        build(m)
        return g.node(name, g.mesh(name, m, mat), translation, rotation, None, parent)

    root = g.node("Rifle")
    part("Receiver", metal, lambda m: m.box((0, 0, 0), (0.07, 0.11, 0.42)), (0, 0.02, -0.1), parent=root)
    part("Barrel", metal, lambda m: m.cylinder_z((0, 0, 0), 0.016, 0.36), (0, 0.045, -0.5), parent=root)
    part("Muzzle Brake", metal, lambda m: m.cylinder_z((0, 0, 0), 0.023, 0.07), (0, 0.045, -0.72), parent=root)
    part("Handguard", polymer, lambda m: m.box((0, 0, 0), (0.064, 0.075, 0.26)), (0, 0.035, -0.42), parent=root)
    part("Stock", polymer, lambda m: m.box((0, 0, 0), (0.05, 0.09, 0.22)), (0, 0.0, 0.22), parent=root)
    part("Butt Pad", dark, lambda m: m.box((0, 0, 0), (0.056, 0.13, 0.03)), (0, -0.01, 0.345), parent=root)
    part("Grip", dark, lambda m: m.box((0, 0, 0), (0.04, 0.12, 0.05)), (0, -0.085, 0.04), quat_x(-15), parent=root)
    part("Magazine", metal, lambda m: m.box((0, 0, 0), (0.04, 0.17, 0.075)), (0, -0.1, -0.17), quat_x(12), parent=root)
    part("Rail", dark, lambda m: m.box((0, 0, 0), (0.03, 0.015, 0.30)), (0, 0.083, -0.12), parent=root)
    sight = part("Sight", metal, lambda m: m.box((0, 0, 0), (0.036, 0.045, 0.06)), (0, 0.11, -0.05), parent=root)
    part("Lens", dot, lambda m: m.box((0, 0, 0), (0.02, 0.02, 0.004)), (0, 0.005, -0.031), parent=sight)
    g.save_gltf("models/rifle.gltf", "models/rifle.bin")


def model_drone(panel_png):
    g = GltfBuilder()
    tex = g.embedded_png(panel_png)
    hull = g.material("Hull", (0.78, 0.80, 0.84), metallic=0.6, roughness=0.38, texture=tex)
    dark = g.material("Frame", (0.08, 0.08, 0.09), metallic=0.5, roughness=0.5)
    eye = g.material("Eye", (1.0, 0.15, 0.1), emissive=(6.0, 0.4, 0.25))
    thruster = g.material("Thruster", (1.0, 0.5, 0.2), emissive=(4.0, 1.5, 0.4))

    def mesh(name, mat, build):
        m = MeshData()
        build(m)
        return g.mesh(name, m, mat)

    root = g.node("Drone")
    g.node("Body", mesh("Body", hull, lambda m: m.sphere((0, 0, 0), 0.45)), None, None, (1.0, 0.62, 1.0), root)
    g.node("Rim", mesh("Rim", dark, lambda m: m.cylinder_y((0, 0, 0), 0.6, 0.07, 32)), parent=root)
    g.node("Eye", mesh("Eye", eye, lambda m: m.sphere((0, 0, 0), 0.13, 16, 10)), (0, 0.02, -0.36), parent=root)
    for side in (-1, 1):
        pod = g.node("Pod", mesh("Pod", dark, lambda m: m.box((0, 0, 0), (0.17, 0.17, 0.5))), (0.66 * side, 0, 0), parent=root)
        g.node("Thruster", mesh("Thruster", thruster, lambda m: m.box((0, 0, 0), (0.11, 0.04, 0.3))), (0, -0.1, 0), parent=pod)
    g.node("Antenna", mesh("Antenna", dark, lambda m: m.box((0, 0, 0), (0.02, 0.25, 0.02))), (0, 0.38, 0.12), parent=root)
    g.save_glb("models/drone.glb")


# ---------------------------------------------------------------------------
# Materials (.mat JSON)
# ---------------------------------------------------------------------------
def material(name, shader="Lit", albedo=(1, 1, 1, 1), albedo_map="", metallic=0.0, roughness=0.6,
             emission=(0, 0, 0), tiling=(1, 1), world_uv=False):
    data = {"Shader": shader, "Albedo": list(albedo), "Albedo Map": albedo_map, "Metallic": metallic,
            "Roughness": roughness, "Emission": list(emission), "Tiling": list(tiling), "World Space UV": world_uv,
            "Name": name}
    with open(path("materials", name + ".mat"), "w") as f:
        json.dump(data, f, indent=2)
    print("  material  materials/%s.mat" % name)


def materials():
    material("Floor", albedo=(0.85, 0.85, 0.85, 1), albedo_map="textures/grid.png", roughness=0.75, tiling=(0.25, 0.25), world_uv=True)
    material("Wall", albedo=(0.78, 0.78, 0.80, 1), albedo_map="textures/concrete.png", roughness=0.9, tiling=(0.2, 0.2), world_uv=True)
    material("Metal", albedo=(0.8, 0.82, 0.85, 1), albedo_map="textures/metal.png", metallic=0.75, roughness=0.38, tiling=(0.5, 0.5), world_uv=True)
    material("Hazard", albedo=(1, 1, 1, 1), albedo_map="textures/hazard.png", roughness=0.6, tiling=(0.5, 0.5), world_uv=True)
    material("Trim", albedo=(0.95, 0.45, 0.10, 1), metallic=0.1, roughness=0.5)
    material("Dark", albedo=(0.12, 0.12, 0.13, 1), metallic=0.4, roughness=0.45)
    material("GlowCyan", albedo=(0.2, 0.8, 1.0, 1), emission=(0.6, 2.4, 3.2))
    material("GlowOrange", albedo=(1.0, 0.5, 0.15, 1), emission=(3.2, 1.3, 0.3))
    material("Plasma", shader="Unlit", albedo=(1.0, 0.55, 0.25, 1), emission=(5.0, 1.8, 0.6))
    material("MuzzleFlash", shader="Unlit", albedo=(1.0, 0.85, 0.5, 1), emission=(8.0, 5.0, 1.8))
    material("HealthPickup", albedo=(0.25, 0.95, 0.35, 1), roughness=0.3, emission=(0.3, 2.0, 0.5))
    material("AmmoPickup", albedo=(0.95, 0.75, 0.2, 1), metallic=0.5, roughness=0.35, emission=(1.6, 1.1, 0.2))


def main():
    print("Generating assets into", ROOT)
    tex_grid()
    tex_concrete()
    tex_crate()
    tex_metal()
    tex_hazard()
    panels = tex_drone_panels()
    model_crate()
    model_barrel()
    model_pillar()
    model_muzzleflash()
    model_rifle()
    model_drone(panels)
    materials()
    print("Done.")


if __name__ == "__main__":
    main()
