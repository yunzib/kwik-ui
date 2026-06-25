module;

#include <string>
#include <memory>
#include <vector>
#include "quickjs.h"

export module kwik.element.table;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font; // FontManager
import kwik.element.typed_prop;
import kwik.engine.state_binding;
import kwik.engine.js_value;

import std;

/**
 * @brief 数据表格组件
 *
 * 按 columns 定义渲染表头 + 数据行 + 斑马纹 + grid 边框。
 * 数据由 JS 传入 data 数组，columns 定义列结构。
 * 表头点击不处理排序（由 JS 侧 array.sort() 自行处理）。
 *
 * JS 用法:
 *   Table({
 *     columns: [
 *       { title: "姓名", key: "name", width: 120 },
 *       { title: "年龄", key: "age",  width: 80,  align: "right" },
 *       { title: "邮箱", key: "email", flex: 1 }
 *     ],
 *     data: [
 *       { name: "Alice", age: 28, email: "alice@example.com" },
 *       { name: "Bob",   age: 35, email: "bob@example.com" }
 *     ],
 *     onRowClick: (e) => console.log(e.index, e.row)
 *   })
 */
export class Table : public View {
public:
    Table() = default;

    /**
     * @brief 构造 Table
     * @param vp 通用视图属性
     * @param tp 表格专有属性
     */
    explicit Table(ViewProps vp, TableProps tp) : View(std::move(vp)), tp_(std::move(tp)) {}

    ~Table() override {
        if (!JS_IsUndefined(data_) && !JS_IsNull(data_) && dataCtx_) JS_FreeValue(dataCtx_, data_);
    }

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Table; }
    const TableProps &tableProps() const { return tp_; }

    /**
     * @brief 设置 JS data 数组引用（element_parser 中调用）
     * @param ctx  QuickJS 上下文
     * @param data JS data 数组（增加引用计数后传入）
     *
     * Table 取得 data 的所有权，析构时自动释放。
     */
    void setJSData(JSContext *ctx, JSValue data) {
        dataCtx_ = ctx;
        data_ = data;
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    TableProps tp_;

    // ─── JS 数据引用 ──────────────────────────────────
    JSContext *dataCtx_ = nullptr;
    JSValue data_{JS_UNDEFINED};

    // ─── 字体路径 ─────────────────────────────────────
    std::string fontPath_;

    // ─── 布局缓存 ─────────────────────────────────────
    float contentWidth_ = 0;

    /**
     * @brief 初始化字体路径（懒加载，首次 onDraw 时调用）
     */
    void ensureFontPath();

    /**
     * @brief 计算各列实际宽度（固定宽优先，剩余按 flex 分配）
     * @param availableW 可用宽度
     * @return 各列宽度的 vector
     */
    std::vector<float> calcColumnWidths(float availableW) const;

    /**
     * @brief 获取 JS data 的行数
     */
    int rowCount(JSContext *ctx) const;

    /**
     * @brief 绘制表头行
     */
    void drawHeader(Graphics &g, float x, float y, float w, float h, const std::vector<float> &colWidths,
                    JSContext *ctx);

    /**
     * @brief 绘制数据行
     */
    void drawRow(Graphics &g, float x, float y, float h, const std::vector<float> &colWidths, int rowIndex,
                 JSValue rowObj, bool isStriped, JSContext *ctx);

    /**
     * @brief 触发 onRowClick 回调
     */
    void fireRowClick(JSContext *ctx, int index, JSValue rowObj);
};