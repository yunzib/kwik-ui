/**
 * @file g3d.cppm
 * @brief G3D 3D 绘制组件
 *
 * 轻量 3D 场景: glTF 模型加载 + 内建基本体 (Box/Sphere/Plane),
 * 轨道相机 + Pan 拖拽旋转 + autoRotate。
 *
 * 渲染链路:
 *   onDraw → matx 计算 mvp → Graphics::drawMesh → DrawList(DrawMeshCmd)
 *         → VulkanBackend::drawMesh → MeshRenderer (深度测试管线)
 *
 * JS 用法:
 * @code
 * import { G3D } from 'kwikui';
 * const g = G3D({ id: "g3d1", width: 320, height: 240, background: "#1a1a2e" });
 * g.loadModel("resources/models/cube.glb");
 * g.setColor("#4fc3f7");
 * g.autoRotate(true);
 * g.addBox(1.0);          // 或直接用内建基本体, 不加载模型
 * @endcode
 */

export module kwik.ext.g3d;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.command; // Vertex3D
import kwik.event;          // DispatchEvent (Pan 手势)
import TernMath;

import std;

// ── 内部场景图节点 (非导出, 仅供本模块两个 TU 使用) ──
struct G3DNode {
    std::vector<Vertex3D> vertices;    // 对象空间三角形列表 (该节点全部 primitive 拼接)
    std::vector<G3DNode> children;     // 子节点
    float t[3] = {0, 0, 0};            // 平移
    float r[4] = {0, 0, 0, 1};         // 旋转四元数 (xyzw)
    float s[3] = {1, 1, 1};            // 缩放
    bool hasMaterialColor = false;     // 材质 baseColorFactor 有效 (glTF 材质最小支持)
    Color materialColor{255, 255, 255, 255};    // 材质基础色 (baseColorFactor × 255)
    int imageIndex = -1;               // 材质贴图 image 索引 (-1 = 无贴图; 经 imageTextureIds_ 换 textureId)
};

/**
 * @brief G3D 3D 绘制组件
 *
 * 相机为围绕原点的轨道相机 (yaw/pitch/distance)。
 * Pan 手势水平拖动 → yaw, 垂直拖动 → pitch; 拖拽期间暂停 autoRotate。
 */
export class G3D : public View {
public:
    G3D() = default;
    explicit G3D(ViewProps vp) : View(std::move(vp)) {}
    ~G3D() override;    // 销毁材质贴图纹理 (延迟释放通道)

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override {
        static ElementType id = registerExtensionType("G3D");    // 注册式扩展类型
        return id;
    }

    // ─── 事件 (Pan 拖拽旋转) ──────────────────────────
    bool onEvent(const DispatchEvent &event) override;

    // ─── JS 导出方法 (由 bindings.cpp 的 js_g3d_* 调用) ──
    void loadModel(const std::string &path);
    void clearModel();
    void setColor(const Color &c) {
        color_ = c;
        markDirty();
    }
    void setAutoRotate(bool on) {
        autoRotate_ = on;
        markDirty();
    }
    void rotateTo(float yawRad, float pitchRad);
    /**
     * @brief 添加立方体 (CPU 生成, 每面法线)
     * @param size 边长
     * @param tx   X 轴平移
     * @param ty   Y 轴平移
     * @param tz   Z 轴平移
     */
    void addBox(float size, float tx = 0, float ty = 0, float tz = 0);

    /**
     * @brief 添加 UV 球 (法线 = 归一化位置)
     * @param radius 半径
     * @param slices 经线段数
     * @param stacks 纬线段数
     * @param tx     X 轴平移
     * @param ty     Y 轴平移
     * @param tz     Z 轴平移
     */
    void addSphere(float radius, int slices, int stacks, float tx = 0, float ty = 0, float tz = 0);

    /**
     * @brief 添加水平地面 (法线 +Y)
     * @param w  宽度
     * @param d  深度
     * @param tx X 轴平移
     * @param ty Y 轴平移
     * @param tz Z 轴平移
     */
    void addPlane(float w, float d, float tx = 0, float ty = 0, float tz = 0);

    /**
     * @brief 显示/隐藏坐标系三轴
     * @param on true=显示 (默认开启)
     */
    void setShowAxes(bool on) {
        showAxes_ = on;
        markDirty();
    }

    // ─── 相机 API (轨道相机参数化; 原先距离/FOV/裁剪面全部写死) ──
    /**
     * @brief 设置轨道相机距离 (注视点恒为原点)
     * @param d 距离 (>0; 过近 clamp 到 0.05)
     */
    void setCameraDistance(float d);
    /** @brief 设置垂直 FOV (度, clamp 5..120, 默认 45) */
    void setFov(float deg);
    /**
     * @brief 设置深度裁剪面
     * @param zn 近平面 (>0)
     * @param zf 远平面 (> zn)
     */
    void setClip(float zn, float zf);
    /**
     * @brief 按场景包围球自动取景: 距离 = r/sin(fov/2) × 1.25,
     *        使任意尺寸模型完整入镜 (解决过大模型被远平面裁掉/过小缩成点)
     */
    void frameModel();

    /**
     * @brief 查询是否显示坐标轴
     * @return true=显示
     */
    bool showAxes() const { return showAxes_; }

     /**
     * @brief 属性写入唯一虚入口
     *
     * 命令式路径与 State 增量路径均汇入此处；
     * string 分支=setProp 包装，原生分支=notify 直传。
     */
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    G3DNode root_;                               // 场景根节点 (世界系)
    Color color_ = Color{160, 170, 190, 255};    // 默认颜色 (无材质节点的绘制色)
    float yaw_ = 0.0f;                           // 轨道相机偏航角 (rad)
    float pitch_ = 0.3f;                         // 轨道相机俯仰角 (rad, 限幅 ±1.5)
    float camDist_ = 4.0f;                       // 相机到原点距离
    float fovDeg_ = 45.0f;                       // 垂直 FOV (度)
    float zNear_ = 0.1f;                         // 深度近平面
    float zFar_ = 100.0f;                        // 深度远平面
    float boundingRadius_ = 0.0f;                // 场景包围球半径 (loadModel 计算, frameModel 用)
    bool panning_ = false;                       // Pan 拖拽中 (拖拽时暂停 autoRotate)
    float lastPanX_ = 0.0f, lastPanY_ = 0.0f;    // 上次 Pan 位置
    bool autoRotate_ = true;                     // 自动旋转
    float autoRotateSpeed_ = 0.02f;              // rad/帧

    bool showAxes_ = true;              ///< 显示坐标系三轴
    float axisLength_ = 1.6f;           ///< 轴线长度 (超出常见物体半边长, 露在物体外)
    float axisThickness_ = 0.02f;       ///< 轴线粗细

    // ── 材质贴图 (复用 TextureManager 异步上传, Image 组件同款模式) ──
    struct PendingTexture {
        std::vector<std::uint8_t> rgba;    // 解码像素 (落地回执前保留——失败重试需源数据)
        int w = 0, h = 0;             // 解码尺寸 (int: stb 接口形状)
        std::uint32_t id = 0;              // 已入队纹理 id (0 = 未入队)
        int retries = 0;              // 失败重试计数 (超限弃传)
    };
    std::vector<PendingTexture> pending_;                // 在途贴图 (下标 = glTF imageIndex)
    std::unordered_map<int, std::uint32_t> imageTextureIds_;  // imageIndex → 已落地纹理 id

    /** @brief onDraw 驱动: 未入队贴图入队创建 + 拉取回执 (落地→登记 id 释源 / 失败→重试) */
    void uploadPendingTextures();
    /** @brief 经延迟释放通道销毁全部已建纹理 (reload/析构用, 在飞帧安全) */
    void destroyTextures();
    /** @brief 递归计算场景世界包围球半径 (loadModel 后调用一次) */
    void computeBoundingRadius();

    // ── 绘制辅助 (递归遍历场景) ──
    void drawNode(Graphics &g, const G3DNode &node, const Mat4 &parentWorld, const Mat4 &viewProj);

    /**
     * @brief 绘制坐标系三轴 (细长方体, 世界原点)
     * @param g        图形上下文
     * @param viewProj 视图-投影矩阵 (轴 world=identity, 故 mvp=viewProj)
     * @param viewport 元素屏幕矩形 (mesh 视口)
     */
    void drawAxes(Graphics &g, const Mat4 &viewProj, const Rect &viewport);
};

/** @brief 自注册入口 (App 在 register_kwikui_module 前调用)。 */
export void registerG3DElement();