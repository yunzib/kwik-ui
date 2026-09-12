// 纯逻辑单测（不依赖窗口/GPU）
// 覆盖：Rect 运算 / PropMeta 一致性（双源登记 bug 的回归防线）/ DisplayList 基本操作
// 运行：ctest -R core_tests 或直接执行 kwik_unit_tests
#include <print>

import kwik.core.types;
import kwik.core.prop_meta;
import kwik.animation.engine;
import kwik.render.command;
import kwik.render.command_buffer;

import std;

static int g_failed = 0;
static int g_total = 0;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        ++g_total;                                                                                                     \
        if (!(cond)) {                                                                                                 \
            ++g_failed;                                                                                                \
            std::println("FAIL {}:{}  {}", __FILE__, __LINE__, #cond);                                                 \
        }                                                                                                              \
    } while (0)

static void test_rect() {
    Rect a{0, 0, 100, 100};
    Rect b{50, 50, 100, 100};
    CHECK(a.intersects(b));
    Rect i = a.intersection(b);
    CHECK(i.x == 50 && i.y == 50 && i.width == 50 && i.height == 50);

    Rect c{200, 200, 10, 10};
    CHECK(!a.intersects(c));
    CHECK(a.intersection(c).isEmpty());    // 无交集 → 空矩形

    Rect u = a.unionRect(b);
    CHECK(u.x == 0 && u.y == 0 && u.width == 150 && u.height == 150);
    CHECK(u.unionRect(Rect{}).width == u.width);    // 与空矩形并集 = 自身
}

static void test_prop_meta_consistency() {
    // ① 名字反查关键属性（含液态玻璃三件套）
    CHECK(propIdFromName("backdropBlur") == PropId::backdropBlur);
    CHECK(propIdFromName("backdropRefraction") == PropId::backdropRefraction);
    CHECK(propIdFromName("backdropSpecular") == PropId::backdropSpecular);
    CHECK(propIdFromName("__nonexistent__") == PropId::COUNT);

    // ② 回归防线：x/y 曾双源错位（kLayoutProps 有、meta 标 false → 动画不动）
    CHECK(getPropMeta(PropId::x).layoutAffecting);
    CHECK(getPropMeta(PropId::y).layoutAffecting);
    CHECK(getPropMeta(PropId::width).layoutAffecting);

    // ③ 双向一致性：动画布局表 ⊆ meta 布局标记（且反向）——新增布局属性时两处必须同步
    for (int pi = 0; pi < static_cast<int>(PropId::COUNT); ++pi) {
        auto id = static_cast<PropId>(pi);
        bool inAnimTable = animationPropAffectsLayout(id);
        bool inMeta = getPropMeta(id).layoutAffecting;
        if (inAnimTable != inMeta) {
            ++g_failed;
            std::println("FAIL 布局标记双源不一致: PropId({}) animTable={} meta={}", pi, inAnimTable, inMeta);
            ++g_total;
        }
        ++g_total;
    }
}

static void test_display_list() {
    DisplayList list;
    list.append(FillRectCmd{Rect{0, 0, 10, 10}, Color{255, 0, 0, 255}, BlendMode::SrcOver, Transform2D{}});
    list.append(FillRectCmd{Rect{10, 0, 10, 10}, Color{0, 255, 0, 255}, BlendMode::SrcOver, Transform2D{}});
    CHECK(list.cmdCount() == 2);

    list.unionBounds(Rect{0, 0, 20, 10});
    CHECK(list.bounds().width == 20);

    auto sub = std::make_shared<const DisplayList>();
    list.appendSubtree(sub);
    CHECK(list.subtreeCount() == 1);

    list.clearSubtreeRefs();    // 只清引用段，图元保留
    CHECK(list.subtreeCount() == 0 && list.cmdCount() == 2);

    list.clear();    // 全清（容量复用）
    CHECK(list.cmdCount() == 0 && list.bounds().isEmpty());
}

int main() {
    test_rect();
    test_prop_meta_consistency();
    test_display_list();
    std::println("[tests] total={} failed={}", g_total, g_failed);
    return g_failed > 0 ? 1 : 0;
}
