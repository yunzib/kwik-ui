# 生成 docsite Image 页测试图：纯标准库手写 PNG 编码（RGB 无 alpha，保证完全不透明）
# kwik_test_warm.png：对角暖渐变 + 白色斜条纹 + 四角红色标记块（裁角演示）
# kwik_test_cool.png：垂直冷渐变 + 白色网格 + 白圆（淡出演示）
import zlib, struct, os

W, H = 320, 220
OUT = os.path.join(os.path.dirname(__file__), '..', 'test', 'image')

def lerp(a, b, t): return int(a + (b - a) * t)

def png_write(path, w, h, pix):  # pix[y][x] = (r,g,b)
    raw = b''.join(b'\x00' + b''.join(struct.pack('BBB', *pix[y][x]) for x in range(w)) for y in range(h))
    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(raw, 9))
           + chunk(b'IEND', b''))
    with open(path, 'wb') as f: f.write(png)
    print('write', path, os.path.getsize(path), 'bytes')

def in_circle(x, y, cx, cy, r): return (x-cx)**2 + (y-cy)**2 <= r*r
def in_tri(q, a, b, c):
    def s(p1, p2, q): return (p2[0]-p1[0])*(q[1]-p1[1]) - (p2[1]-p1[1])*(q[0]-p1[0])
    d1, d2, d3 = s(a, b, q), s(b, c, q), s(c, a, q)
    neg = (d1 < 0) or (d2 < 0) or (d3 < 0); pos = (d1 > 0) or (d2 > 0) or (d3 > 0)
    return not (neg and pos)

# ── warm：对角渐变（橙→品红）+ 斜条纹 + 四角红块 ──
pix = []
for y in range(H):
    row = []
    for x in range(W):
        t = (x + y) / (W + H)
        r, g, b = lerp(244, 168, t), lerp(114, 85, t), lerp(32, 182, t)
        if (x + y * 2) % 56 < 9: r, g, b = 255, 255, 255          # 白斜条纹
        m = 22                                                    # 四角红标记块
        if (x < m or x >= W - m) and (y < m or y >= H - m): r, g, b = 220, 38, 38
        row.append((r, g, b))
    pix.append(row)
png_write(os.path.join(OUT, 'kwik_test_warm.png'), W, H, pix)

# ── cool：垂直渐变（青→深蓝）+ 网格 + 白圆 + 橙三角 ──
pix = []
for y in range(H):
    row = []
    for x in range(W):
        t = y / H
        r, g, b = lerp(34, 15, t), lerp(197, 68, t), lerp(194, 118, t)
        if x % 40 < 2 or y % 40 < 2: r, g, b = lerp(255,180,t), lerp(255,220,t), 255  # 网格
        if in_circle(x, y, W//2, H//2, 62): r, g, b = 255, 255, 255                   # 白圆
        if in_tri((x, y), (0, 0), (W, H), (W, H - 90)): r, g, b = 249, 154, 39       # 橙三角
        row.append((r, g, b))
    pix.append(row)
png_write(os.path.join(OUT, 'kwik_test_cool.png'), W, H, pix)
