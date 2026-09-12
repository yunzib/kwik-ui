// 纯逻辑单测（不依赖窗口/GPU）
// 覆盖：Rect 运算 / PropMeta 表完整性与行为锁（原双源登记 bug 的回归防线）/ DisplayList 基本操作
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
    // ① 名字反查关键属性（含液态玻璃三件套与别名）
    CHECK(propIdFromName("backdropBlur") == PropId::backdropBlur);
    CHECK(propIdFromName("backdropRefraction") == PropId::backdropRefraction);
    CHECK(propIdFromName("backdropSpecular") == PropId::backdropSpecular);
    CHECK(propIdFromName("bg") == PropId::background);
    CHECK(propIdFromName("w") == PropId::width);
    CHECK(propIdFromName("__nonexistent__") == PropId::COUNT);

    // ② 布局属性行为锁：Layout 标志必须恰好钉在这 10 个属性上
    //    （防误改标志改变 relayout 行为；x/y/absTop 双源事故的回归防线）
    const char *kLayoutNames[] = {"width", "height", "padding", "margin",
                                  "x", "y", "absTop", "absLeft", "absRight", "absBottom"};
    for (int pi = 0; pi < static_cast<int>(PropId::COUNT); ++pi) {
        auto id = static_cast<PropId>(pi);
        bool isLayout = getPropMeta(id).flags & PropFlags::Layout;
        bool inLock = false;
        for (auto *n : kLayoutNames) inLock = inLock || std::string_view(propName(id)) == n;
        if (isLayout != inLock) {
            ++g_failed;
            std::println("FAIL 布局标志与行为锁不符: {} expected={} actual={}",
                         propName(id), inLock, isLayout);
        }
        ++g_total;
    }
    CHECK(animationPropAffectsLayout(PropId::x));        // 导出函数与 meta 同源可用
    CHECK(!animationPropAffectsLayout(PropId::opacity));

    // ③ 全表巡检：正名可反查、名字/别名全表无冲突、reader 有值（shadow 桩除外）
    for (int pi = 0; pi < static_cast<int>(PropId::COUNT); ++pi) {
        auto id = static_cast<PropId>(pi);
        const auto &m = getPropMeta(id);
        CHECK(m.name != nullptr && *m.name != '\0');
        if (propIdFromName(m.name) != id) {
            ++g_failed;
            std::println("FAIL 正名反查失败: {} -> PropId({})", m.name, static_cast<int>(propIdFromName(m.name)));
        }
        ++g_total;
        if (id != PropId::shadow && !m.reader) {
            ++g_failed;
            std::println("FAIL reader 缺失: {}", m.name);
        }
        ++g_total;
    }
    // 别名不与任何正名冲突（正名优先级高，冲突别名永远不可达 = 登记错误）
    CHECK(propIdFromName("background") == PropId::background);    // "background" 不是别的属性的别名
    CHECK(propIdFromName("radius") == PropId::borderRadius);
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
