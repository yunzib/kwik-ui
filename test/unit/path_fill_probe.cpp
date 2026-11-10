// 探针：验证 triangulateFill ear clipping 剖分（P1-4 凹多边形填充）。
// 正确性判据（与剖分算法无关的独立口径）：
//   1. 三角形面积和 == 多边形 shoelace 面积（剖分不重不漏的充要近似）
//   2. 三角形数 == n - 2（简单多边形剖分三角数恒等式）
//   3. edgeMask：轮廓边置位、剖分对角线（非相邻顶点边）清零
// 全 PASS 退出码 0。
import std;
import kwik.core.path;

#include <cstddef>

static int failures = 0;

static void expectBool(const char *desc, bool expect, bool actual) {
    bool pass = expect == actual;
    if (!pass) ++failures;
    std::printf("[%s] %s\n", pass ? "PASS" : "FAIL", desc);
}

static void expectNear(const char *desc, float expect, float actual, float eps) {
    bool pass = std::fabs(expect - actual) <= eps;
    if (!pass) ++failures;
    std::printf("[%s] %-44s expect=%.4f actual=%.4f\n", pass ? "PASS" : "FAIL", desc, expect, actual);
}

static void expectTrue(const char *desc, bool actual) {
    if (!actual) ++failures;
    std::printf("[%s] %s\n", actual ? "PASS" : "FAIL", desc);
}

// ── 独立口径工具 ──
static float triAreaSum(const std::vector<Triangle> &tris) {
    float s = 0.0f;
    for (auto &t : tris) s += std::fabs((t.p1.x - t.p0.x) * (t.p2.y - t.p0.y) - (t.p1.y - t.p0.y) * (t.p2.x - t.p0.x)) * 0.5f;
    return s;
}

static float shoelaceArea(const std::vector<Vec2> &pts) {
    float a2 = 0.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
        const Vec2 &a = pts[i];
        const Vec2 &b = pts[(i + 1) % pts.size()];
        a2 += a.x * b.y - b.x * a.y;
    }
    return std::fabs(a2) * 0.5f;
}

static Path polyPath(const std::vector<Vec2> &pts) {
    Path p;
    p.moveTo(pts[0].x, pts[0].y);
    for (size_t i = 1; i < pts.size(); ++i) p.lineTo(pts[i].x, pts[i].y);
    p.closePath();
    return p;
}

// 轮廓边置位总数应等于多边形边数 n（每条轮廓边恰被一个三角形以轮廓边身份持有）
static int outlineEdgeCount(const std::vector<Triangle> &tris) {
    int cnt = 0;
    for (auto &t : tris) {
        if (t.edgeMask & 0b001) ++cnt;
        if (t.edgeMask & 0b010) ++cnt;
        if (t.edgeMask & 0b100) ++cnt;
    }
    return cnt;
}

int main() {
    // ── ① 凸四边形（单位正方形）：2 三角形，面积 1，对角线为内部边 ──
    {
        std::vector<Vec2> sq = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        auto tris = triangulateFill(polyPath(sq));
        expectBool("凸四边形 三角形数==2", true, tris.size() == 2);
        expectNear("凸四边形 面积和==1", 1.0f, triAreaSum(tris), 1e-4f);
        expectNear("凸四边形 shoelace==1", 1.0f, shoelaceArea(sq), 1e-6f);
        expectTrue("凸四边形 轮廓边计数==4", outlineEdgeCount(tris) == 4);
    }

    // ── ② 凹多边形（L 形，6 顶点）：4 三角形，面积和==shoelace ──
    //     旧形心扇在 L 形上形心可能落外形 → 覆盖失真（本剖分核心修复点）
    {
        std::vector<Vec2> ell = {{0, 0}, {4, 0}, {4, 1}, {1, 1}, {1, 4}, {0, 4}};
        auto tris = triangulateFill(polyPath(ell));
        expectBool("L形凹多边形 三角形数==4", true, tris.size() == 4);
        expectNear("L形 面积和==shoelace", shoelaceArea(ell), triAreaSum(tris), 1e-4f);
        expectTrue("L形 轮廓边计数==6", outlineEdgeCount(tris) == 6);
    }

    // ── ③ 深凹角（凹点深入到会落进朴素扇三角形内的位置——containment 测试生效点）──
    //     5 顶点：矩形底边中点向上挖出深 V 缺口
    {
        std::vector<Vec2> notch = {{0, 0}, {4, 0}, {4, 4}, {2, 3}, {0, 4}};
        auto tris = triangulateFill(polyPath(notch));
        expectBool("深V缺口 三角形数==3", true, tris.size() == 3);
        expectNear("深V缺口 面积和==shoelace", shoelaceArea(notch), triAreaSum(tris), 1e-4f);
        expectTrue("深V缺口 轮廓边计数==5", outlineEdgeCount(tris) == 5);
    }

    // ── ④ 星形（10 顶点凹多边形）：8 三角形 ──
    {
        std::vector<Vec2> star;
        for (int i = 0; i < 10; ++i) {
            float a = static_cast<float>(i) * 3.14159265f / 5.0f;
            float r = (i % 2 == 0) ? 5.0f : 2.0f;
            star.push_back({5.0f + std::cos(a) * r, 5.0f + std::sin(a) * r});
        }
        auto tris = triangulateFill(polyPath(star));
        expectBool("星形 三角形数==8", true, tris.size() == 8);
        expectNear("星形 面积和==shoelace", shoelaceArea(star), triAreaSum(tris), 1e-3f);
        expectTrue("星形 轮廓边计数==10", outlineEdgeCount(tris) == 10);
    }

    // ── ⑤ 顺时针环绕（CW 输入 → 统一 CCW 后面积不变）──
    {
        std::vector<Vec2> cw = {{0, 0}, {0, 2}, {3, 2}, {3, 0}};
        auto tris = triangulateFill(polyPath(cw));
        expectNear("CW环绕 面积和==shoelace", shoelaceArea(cw), triAreaSum(tris), 1e-4f);
        expectBool("CW环绕 三角形数==2", true, tris.size() == 2);
    }

    // ── ⑥ 圆弧轮廓（Path.arc + closePath，n 大且含闭合重复点/密共线点）──
    // 注：arc 细分为 ~19 边内接多边形，弦近似天然低于 πr² 约 1.8%
    // （0.5·n·r²·sin(2π/n)），容差 2.5% 只锁「剖分覆盖整个多边形」
    {
        Path p;
        p.arc(5, 5, 3, 0, 6.2831853f, false);
        p.closePath();
        auto tris = triangulateFill(p);
        float expect = 3.14159265f * 9.0f;
        expectNear("圆弧 面积和≈πr²", expect, triAreaSum(tris), expect * 0.025f);
    }

    // ── ⑦ 共线点去重（正方形边中点插点 → 不死循环、面积不变）──
    {
        std::vector<Vec2> coll = {{0, 0}, {2, 0}, {2, 2}, {1, 2}, {0, 2}};
        auto tris = triangulateFill(polyPath(coll));
        expectNear("共线点去重 面积和==4", 4.0f, triAreaSum(tris), 1e-4f);
        expectTrue("共线点去重 轮廓边计数==4", outlineEdgeCount(tris) == 4);
    }

    // ── ⑧ 多轮廓独立填充（两个不相交方框）──
    {
        Path p;
        p.moveTo(0, 0); p.lineTo(1, 0); p.lineTo(1, 1); p.lineTo(0, 1); p.closePath();
        p.moveTo(5, 0); p.lineTo(6, 0); p.lineTo(6, 1); p.lineTo(5, 1); p.closePath();
        auto tris = triangulateFill(p);
        expectNear("双轮廓 面积和==2", 2.0f, triAreaSum(tris), 1e-4f);
        expectBool("双轮廓 三角形数==4", true, tris.size() == 4);
    }

    if (failures == 0) {
        std::printf("\nALL PASS (0 failures)\n");
        return 0;
    }
    std::printf("\nFAILURES: %d\n", failures);
    return 1;
}
