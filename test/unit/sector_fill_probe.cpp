// 探针：饼图扇形（moveTo 圆心 → arc → closePath → fill）覆盖缺口排查。
// 用户报告：g2d 示例④饼图扇形之间出现白色楔形缺口 + 斜边锯齿 + 圆弧边缘小凸点。
// 本探针在 CPU 侧复现示例④的三个扇形路径，跑 triangulateFill(dilate)，
// 软件栅格化三角形覆盖，与「原始扇形区域」逐采样点比对 —— 定位缺口是否
// 出自 CPU 几何（剖分/外扩），还是 GPU 管线（顶点展开/shader）。
// 全 PASS 退出码 0。
import std;
import kwik.core.path;


static int failures = 0;

static void expectTrue(const char *desc, bool actual) {
    if (!actual) ++failures;
    std::printf("[%s] %s\n", actual ? "PASS" : "FAIL", desc);
}

static float triAreaSum(const std::vector<Triangle> &tris) {
    float s = 0.0f;
    for (auto &t : tris)
        s += std::fabs((t.p1.x - t.p0.x) * (t.p2.y - t.p0.y) -
                       (t.p1.y - t.p0.y) * (t.p2.x - t.p0.x)) * 0.5f;
    return s;
}

// 与示例④一致的扇形路径
static Path sectorPath(float cx, float cy, float r, float a0, float a1) {
    Path p;
    p.moveTo(cx, cy);
    p.arc(cx, cy, r, a0, a1, false);
    p.closePath();
    return p;
}

// 原始扇形区域判定（无外扩）：点到圆心距离 <= r 且角度在 [a0,a1] 内（y-down，
// 角度按 atan2(dy,dx) 归一到 [0,2π)，支持跨 0/2π）
static bool inSector(float x, float y, float cx, float cy, float r, float a0, float a1) {
    float dx = x - cx, dy = y - cy;
    if (dx * dx + dy * dy > r * r) return false;
    float ang = std::atan2(dy, dx);
    if (ang < 0.0f) ang += std::numbers::pi_v<float> * 2.0f;
    float s = a0, e = a1;
    auto norm = [](float a) {
        float t = std::numbers::pi_v<float> * 2.0f;
        a = std::fmod(a, t);
        if (a < 0.0f) a += t;
        return a;
    };
    s = norm(s); e = norm(e); ang = norm(ang);
    if (s <= e) return ang >= s - 1e-4f && ang <= e + 1e-4f;
    return ang >= s - 1e-4f || ang <= e + 1e-4f;    // 跨 0°
}

// 软件栅格化：采样点被任一三角形覆盖（重心坐标全 >= -1e-6）即算覆盖
static bool coveredByTris(float x, float y, const std::vector<Triangle> &tris) {
    for (auto &t : tris) {
        float e1x = t.p1.x - t.p0.x, e1y = t.p1.y - t.p0.y;
        float e2x = t.p2.x - t.p0.x, e2y = t.p2.y - t.p0.y;
        float det = e1x * e2y - e1y * e2x;
        if (std::fabs(det) < 1e-12f) continue;
        float px = x - t.p0.x, py = y - t.p0.y;
        float u = (px * e2y - py * e2x) / det;
        float v = (e1x * py - e1y * px) / det;
        float w = 1.0f - u - v;
        if (u >= -1e-4f && v >= -1e-4f && w >= -1e-4f) return true;
    }
    return false;
}

static void probeSector(const char *name, float a0, float a1, float dilate) {
    const float cx = 90.0f, cy = 72.0f, r = 52.0f;
    Path p = sectorPath(cx, cy, r, a0, a1);
    auto tris = triangulateFill(p, dilate);

    // 剖分三角数恒等式：n 边形 → n-2
    std::size_t contourPts = 0;
    for (auto &c : p.contours()) contourPts += c.points.size();
    expectTrue((std::string(name) + " 三角数 == n-2").c_str(),
               tris.size() == contourPts - 2);

    // 面积守恒：三角面积和 ≈ 扇形面积（0.5·r²·Δθ）+ 外扩余量（dilate 只增不减）
    float area = triAreaSum(tris);
    float span = a1 - a0;
    float sectorArea = 0.5f * r * r * span;
    std::printf("       %s: tris=%zu areaSum=%.2f sectorArea=%.2f\n", name,
                tris.size(), area, sectorArea);
    expectTrue((std::string(name) + " 面积 >= 扇形面积").c_str(), area >= sectorArea * 0.999f);

    // 覆盖缺口扫描：原始扇形区域内 1px 步长采样，未覆盖数应为 0。
    // dilate=0 对照组：弧边界半径处弦线逼近固有落差（弦在真弧内侧），
    // 只检查半径 ≤ r-2 的内部区域
    int total = 0, missed = 0;
    float maxMissDist = 0.0f;
    float rMax = (dilate > 0.0f) ? r + 2.0f : r - 2.0f;
    for (float y = cy - rMax; y <= cy + rMax; y += 1.0f) {
        for (float x = cx - rMax; x <= cx + rMax; x += 1.0f) {
            float rad = std::hypot(x - cx, y - cy);
            if (rad > rMax) continue;
            if (!inSector(x, y, cx, cy, r, a0, a1)) continue;
            ++total;
            if (!coveredByTris(x, y, tris)) {
                ++missed;
                maxMissDist = std::max(maxMissDist, rad);
            }
        }
    }
    std::printf("       %s: 原始区域内采样=%d 未覆盖=%d (最远缺口距圆心=%.1fpx)\n",
                name, total, missed, maxMissDist);
    expectTrue((std::string(name) + " 原始扇形区域无覆盖缺口").c_str(), missed == 0);
}

int main() {
    std::printf("=== 饼图扇形覆盖探针（示例④）===\n");
    const float pi = std::numbers::pi_v<float>;

    // ── 调试：绿色扇形轮廓与三角形明细 ──
    {
        Path p = sectorPath(90.0f, 72.0f, 52.0f, pi * 1.7f, pi * 2.0f);
        for (auto &c : p.contours()) {
            std::printf("轮廓: closed=%d points=%zu\n", c.closed ? 1 : 0, c.points.size());
            if (!c.points.empty()) {
                auto &pts = c.points;
                std::printf("  first=(%.2f,%.2f) second=(%.2f,%.2f) last=(%.2f,%.2f)\n",
                            pts[0].x, pts[0].y,
                            pts.size() > 1 ? pts[1].x : 0.f, pts.size() > 1 ? pts[1].y : 0.f,
                            pts.back().x, pts.back().y);
            }
        }
        auto tris = triangulateFill(p, 0.0f);
        std::printf("绿扇形 tris=%zu，按面积降序前 8 个:\n", tris.size());
        std::vector<std::pair<float, std::size_t>> order;
        for (std::size_t i = 0; i < tris.size(); ++i) {
            auto &t = tris[i];
            float a = std::fabs((t.p1.x - t.p0.x) * (t.p2.y - t.p0.y) -
                                (t.p1.y - t.p0.y) * (t.p2.x - t.p0.x)) * 0.5f;
            order.push_back({a, i});
        }
        std::sort(order.rbegin(), order.rend());
        for (std::size_t k = 0; k < order.size() && k < 8; ++k) {
            auto &t = tris[order[k].second];
            std::printf("  area=%9.3f  mask=%d  (%.1f,%.1f)-(%.1f,%.1f)-(%.1f,%.1f)\n",
                        order[k].first, t.edgeMask,
                        t.p0.x, t.p0.y, t.p1.x, t.p1.y, t.p2.x, t.p2.y);
        }
    }

    // 示例④三扇形：dilate = 1/1.5（sc=1.5 时 fillPath 实传值）
    probeSector("红[0,0.9π] dilate=0.667",        0.0f,          pi * 0.9f, 1.0f / 1.5f);
    probeSector("橙[0.9π,1.7π] dilate=0.667",     pi * 0.9f,     pi * 1.7f, 1.0f / 1.5f);
    probeSector("绿[1.7π,2π] dilate=0.667",       pi * 1.7f,     pi * 2.0f, 1.0f / 1.5f);
    // 对照：dilate=0（外扩关闭）同样必须无缺口
    probeSector("红[0,0.9π] dilate=0",            0.0f,          pi * 0.9f, 0.0f);
    probeSector("橙[0.9π,1.7π] dilate=0",         pi * 0.9f,     pi * 1.7f, 0.0f);
    probeSector("绿[1.7π,2π] dilate=0",           pi * 1.7f,     pi * 2.0f, 0.0f);

    std::printf("\n%s (%d failures)\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
