// ============================================================================
// g3d.cpp — G3D 3D 绘制组件实现
//
// 每帧: 计算轨道相机 view/proj → 递归遍历场景节点累积世界矩阵
//       → drawMesh (对象空间顶点 + mvp + 颜色 + 对象空间方向光)
// autoRotate 采用 Spinner 的 onDraw 自增 + markDirty 动画模式。
// ============================================================================

module;

#include <cstring>
#include <cmath>
#include "kwik/g3d_gltf.h"

module kwik.ext.g3d;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.command;
import kwik.event;
import kwik.core.log;
import TernMath;

import std;

namespace {

// ── 四元数 (xyzw) → 列主序旋转矩阵 (3×3 展开到 Mat4) ──
Mat4 quatToMat(const float q[4]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;
    Mat4 m;
    // 列主序: m[col*4+row]
    m.m[0] = 1 - (yy + zz);
    m.m[4] = xy - wz;
    m.m[8] = xz + wy;
    m.m[1] = xy + wz;
    m.m[5] = 1 - (xx + zz);
    m.m[9] = yz - wx;
    m.m[2] = xz - wy;
    m.m[6] = yz + wx;
    m.m[10] = 1 - (xx + yy);
    return m;
}

// ── 生成一个四边形面 (中心 c, 法线 n, 两个切向 u/w, 边长 s) ──
void pushQuad(std::vector<Vertex3D> &v, const Vec3 &n, const Vec3 &c, const Vec3 &u, const Vec3 &w, float s) {
    Vec3 a = c - u * (s * 0.5f) - w * (s * 0.5f);
    Vec3 b = c + u * (s * 0.5f) - w * (s * 0.5f);
    Vec3 d = c - u * (s * 0.5f) + w * (s * 0.5f);
    Vec3 e = c + u * (s * 0.5f) + w * (s * 0.5f);
    auto emit = [&](const Vec3 &p) { v.push_back({p.x, p.y, p.z, n.x, n.y, n.z}); };
    emit(a);
    emit(b);
    emit(d);
    emit(b);
    emit(e);
    emit(d);
}

// ── glTF 加载结果 → 内部节点树 (G3DVertex → Vertex3D 类型转换) ──
void convertModel(const G3DModel &model, const G3DModel::G3DNode &src, G3DNode &dst) {
    dst.t[0] = src.translation[0];
    dst.t[1] = src.translation[1];
    dst.t[2] = src.translation[2];
    dst.r[0] = src.rotation[0];
    dst.r[1] = src.rotation[1];
    dst.r[2] = src.rotation[2];
    dst.r[3] = src.rotation[3];
    dst.s[0] = src.scale[0];
    dst.s[1] = src.scale[1];
    dst.s[2] = src.scale[2];
    if (src.meshIndex >= 0 && src.meshIndex < static_cast<int>(model.meshes.size())) {
        const auto &md = model.meshes[src.meshIndex];
        dst.vertices.reserve(md.vertices.size());
        for (const auto &srcV : md.vertices)
            dst.vertices.push_back({srcV.x, srcV.y, srcV.z, srcV.nx, srcV.ny, srcV.nz});
    }
    for (const auto &child : src.children) {
        dst.children.emplace_back();
        convertModel(model, child, dst.children.back());
    }
}

// ── 矩形四边形面 (与 pushQuad 同构, 但 u/w 方向尺寸可不同; 用于非正方体面) ──
void pushQuadRect(std::vector<Vertex3D> &v, const Vec3 &n, const Vec3 &c, const Vec3 &u, const Vec3 &w,
                  float su, float sw) {
    Vec3 a = c - u * (su * 0.5f) - w * (sw * 0.5f);
    Vec3 b = c + u * (su * 0.5f) - w * (sw * 0.5f);
    Vec3 d = c - u * (su * 0.5f) + w * (sw * 0.5f);
    Vec3 e = c + u * (su * 0.5f) + w * (sw * 0.5f);
    auto emit = [&](const Vec3 &p) { v.push_back({p.x, p.y, p.z, n.x, n.y, n.z}); };
    emit(a);
    emit(b);
    emit(d);
    emit(b);
    emit(e);
    emit(d);
}

// ── 轴对齐长方体 (min/max 对角点; 6 面矩形, 每面法线) ──
void pushBox3D(std::vector<Vertex3D> &v, const Vec3 &min, const Vec3 &max) {
    Vec3 c{(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f};
    Vec3 h{(max.x - min.x) * 0.5f, (max.y - min.y) * 0.5f, (max.z - min.z) * 0.5f};
    // 6 个面: 法线 + 中心 + 两个切向, 尺寸 = 各自方向半边长×2
    pushQuadRect(v, {0, 0, 1}, {c.x, c.y, c.z + h.z}, {1, 0, 0}, {0, 1, 0}, h.x * 2, h.y * 2);
    pushQuadRect(v, {0, 0, -1}, {c.x, c.y, c.z - h.z}, {-1, 0, 0}, {0, 1, 0}, h.x * 2, h.y * 2);
    pushQuadRect(v, {1, 0, 0}, {c.x + h.x, c.y, c.z}, {0, 0, 1}, {0, 1, 0}, h.z * 2, h.y * 2);
    pushQuadRect(v, {-1, 0, 0}, {c.x - h.x, c.y, c.z}, {0, 0, -1}, {0, 1, 0}, h.z * 2, h.y * 2);
    pushQuadRect(v, {0, 1, 0}, {c.x, c.y + h.y, c.z}, {1, 0, 0}, {0, 0, 1}, h.x * 2, h.z * 2);
    pushQuadRect(v, {0, -1, 0}, {c.x, c.y - h.y, c.z}, {1, 0, 0}, {0, 0, -1}, h.x * 2, h.z * 2);
}

}    // namespace

// ============================================================================
// onMeasure — 默认 300×150 (同 G2D / Web Canvas 默认值)
// ============================================================================
Size G3D::onMeasure(Constraints constraints) {
    float w = props.width.value_or(300.0f);
    float h = props.height.value_or(150.0f);
    return constraints.constrain({w, h});
}

// ============================================================================
// onDraw — 相机 → 递归绘制场景 → autoRotate
// ============================================================================
void G3D::onDraw(Graphics &graphics) {
    View::onDraw(graphics);
    if (root_.vertices.empty() && root_.children.empty()) return;    // 无模型不绘制

    graphics.save();

    // ── 轨道相机: 眼睛绕原点旋转, 注视原点 ──
    float cp = std::cos(pitch_);
    Vec3 eye{camDist_ * cp * std::cos(yaw_), camDist_ * std::sin(pitch_), camDist_ * cp * std::sin(yaw_)};
    Mat4 view = lookAt(eye, {0, 0, 0}, {0, 1, 0});

    // ── 投影: 45° FOV, NDC z∈[0,1] (Vulkan)
    //    乘 scale(1,-1,1) 完成 y 翻转 — 渲染器用正高度 viewport
    //    (见 vulkan_3d_renderer.cpp 注释)
    float aspect = (frame.height > 1e-3f) ? frame.width / frame.height : 1.0f;
    float fov = 45.0f * 3.14159265f / 180.0f;
    Mat4 proj = mul(scale(1, -1, 1), perspective(fov, aspect, 0.1f, 100.0f));
    Mat4 viewProj = mul(proj, view);

    // ── 坐标轴: 先画, 物体后画遮挡原点交汇段, 轴尾露在物体外 ──
    if (showAxes_) drawAxes(graphics, viewProj, frame);

    Mat4 identity = Mat4::identity();
    drawNode(graphics, root_, identity, viewProj);

    graphics.restore();

    // ── autoRotate: 每帧自增 yaw 并保持脏 (拖拽中暂停, 与用户操作不打架) ──
    if (autoRotate_ && !panning_) {
        yaw_ += autoRotateSpeed_;
        markDirty();
    }
}

// ============================================================================
// drawNode — 递归累积世界矩阵并绘制节点网格
// ============================================================================
void G3D::drawNode(Graphics &g, const G3DNode &node, const Mat4 &parentWorld, const Mat4 &viewProj) {
    // 本地变换 = T * R * S
    Mat4 local =
        mul(mul(translate(node.t[0], node.t[1], node.t[2]), quatToMat(node.r)), scale(node.s[0], node.s[1], node.s[2]));
    Mat4 world = mul(parentWorld, local);

    if (!node.vertices.empty()) {
        Mat4 mvp = mul(viewProj, world);

        // ── 方向光: 视图空间固定 normalize(0.3,0.6,0.7)
        //    对象空间 = R^T * l (R 为 world 旋转部分)
        //    对"旋转 + 均匀缩放"的节点精确; 非均匀缩放的近似误差 v1 可接受
        float lv[3] = {0.3f, 0.6f, 0.7f};
        float lo[3];
        lo[0] = lv[0] * world.m[0] + lv[1] * world.m[1] + lv[2] * world.m[2];
        lo[1] = lv[0] * world.m[4] + lv[1] * world.m[5] + lv[2] * world.m[6];
        lo[2] = lv[0] * world.m[8] + lv[1] * world.m[9] + lv[2] * world.m[10];

        // ── mvp 直接上传（列主序）: mesh shader (RowMajor + OpVectorTimesMatrix)
        //    已推演该组合下上传列主序 M 即得正确 M·v, 无需转置
        //    viewport=frame: mesh 视口与元素矩形一致, cube 居中于元素内
        float mvpArr[16];
        mvp.toArray(mvpArr);
        g.drawMesh(node.vertices, mvpArr, color_, lo, frame);
    }

    for (const auto &child : node.children) drawNode(g, child, world, viewProj);
}

// ============================================================================
// drawAxes — 坐标系三轴 (X红/Y绿/Z蓝), 世界原点, 细长方体
//   world=identity → mvp=viewProj; 每轴一次 drawMesh 独立着色
// ============================================================================
void G3D::drawAxes(Graphics &g, const Mat4 &viewProj, const Rect &viewport) {
    float L = axisLength_;
    float h = axisThickness_ * 0.5f;
    float lv[3] = {0.3f, 0.6f, 0.7f};    // 对象空间方向光 (轴 world=identity, 同 G3D 默认)
    float mvpArr[16];
    viewProj.toArray(mvpArr);

    std::vector<Vertex3D> xv, yv, zv;
    pushBox3D(xv, {0, -h, -h}, {L, h, h});       // +X 轴 (原点→+X)
    pushBox3D(yv, {-h, 0, -h}, {h, L, h});       // +Y 轴 (原点→+Y)
    pushBox3D(zv, {-h, -h, 0}, {h, h, L});       // +Z 轴 (原点→+Z)

    g.drawMesh(xv, mvpArr, Color{255, 80, 80, 255}, lv, viewport);       // X = 红
    g.drawMesh(yv, mvpArr, Color{80, 220, 80, 255}, lv, viewport);       // Y = 绿
    g.drawMesh(zv, mvpArr, Color{80, 160, 255, 255}, lv, viewport);      // Z = 蓝
}

// ============================================================================
// onEvent — Pan 拖拽 → yaw/pitch 旋转
// ============================================================================
bool G3D::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::PanBegin:
        panning_ = true;
        lastPanX_ = event.globalX;
        lastPanY_ = event.globalY;
        return true;
    case DispatchEvent::Type::PanMove: {
        if (!panning_) break;
        float dx = event.globalX - lastPanX_;
        float dy = event.globalY - lastPanY_;
        lastPanX_ = event.globalX;
        lastPanY_ = event.globalY;
        yaw_ -= dx * 0.01f;                                       // 水平拖动 → 偏航
        pitch_ = std::clamp(pitch_ + dy * 0.01f, -1.5f, 1.5f);    // 垂直拖动 → 俯仰 (±86°)
        markDirty();
        return true;
    }
    case DispatchEvent::Type::PanEnd: panning_ = false; return true;
    default: break;
    }
    return View::onEvent(event);
}

// ============================================================================
// loadModel / clearModel
// ============================================================================
void G3D::loadModel(const std::string &path) {
    G3DModel model;
    std::string err;
    if (!g3dLoadFromFile(path.c_str(), model, err)) {
        Log::error("[G3D] loadModel '{}' failed: {}", path, err);
        return;
    }
    root_.children.clear();
    root_.vertices.clear();
    for (const auto &srcChild : model.root.children) {
        root_.children.emplace_back();
        convertModel(model, srcChild, root_.children.back());
    }
    markDirty();
    Log::info("[G3D] loadModel '{}' ok, root nodes={}", path, root_.children.size());

    Log::info("[G3D] meshPool={}", model.meshes.size());
    for (size_t i = 0; i < model.meshes.size(); ++i)
        Log::info("[G3D] meshPool[{}] verts={}", i, model.meshes[i].vertices.size());
    for (size_t i = 0; i < root_.children.size(); ++i)
        Log::info("[G3D] child[{}] verts={} children={}", i, root_.children[i].vertices.size(),
                  root_.children[i].children.size());
}

void G3D::clearModel() {
    root_.children.clear();
    root_.vertices.clear();
    markDirty();
}

// ============================================================================
// rotateTo — 绝对轨道角度 (弧度)
// ============================================================================
void G3D::rotateTo(float yawRad, float pitchRad) {
    yaw_ = yawRad;
    pitch_ = std::clamp(pitchRad, -1.5f, 1.5f);
    markDirty();
}

// ============================================================================
// 内建基本体 (CPU 生成, 每面法线)
// ============================================================================
void G3D::addBox(float size, float tx, float ty, float tz) {
    G3DNode node;
    float h = size * 0.5f;
    // 6 个面: 法线 + 中心 + 两个切向
    pushQuad(node.vertices, {0, 0, 1}, {0, 0, h}, {1, 0, 0}, {0, 1, 0}, size);
    pushQuad(node.vertices, {0, 0, -1}, {0, 0, -h}, {-1, 0, 0}, {0, 1, 0}, size);
    pushQuad(node.vertices, {1, 0, 0}, {h, 0, 0}, {0, 0, 1}, {0, 1, 0}, size);
    pushQuad(node.vertices, {-1, 0, 0}, {-h, 0, 0}, {0, 0, -1}, {0, 1, 0}, size);
    pushQuad(node.vertices, {0, 1, 0}, {0, h, 0}, {1, 0, 0}, {0, 0, 1}, size);
    pushQuad(node.vertices, {0, -1, 0}, {0, -h, 0}, {1, 0, 0}, {0, 0, -1}, size);
    node.t[0] = tx;
    node.t[1] = ty;
    node.t[2] = tz;
    root_.children.push_back(std::move(node));
    markDirty();
}

// ── UV 球: 法线 = 归一化位置 ──
void G3D::addSphere(float radius, int slices, int stacks, float tx, float ty, float tz) {
    slices = std::max(slices, 3);
    stacks = std::max(stacks, 2);
    G3DNode node;
    float PI = 3.14159265f;
    auto sph = [&](float phi, float theta) {
        return Vec3{radius * std::cos(phi) * std::sin(theta), radius * std::cos(theta),
                    radius * std::sin(phi) * std::sin(theta)};
    };
    auto emit = [&](const Vec3 &p) {
        Vec3 n{p.x / radius, p.y / radius, p.z / radius};
        node.vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z});
    };
    for (int i = 0; i < stacks; i++) {
        float th0 = PI * float(i) / stacks;
        float th1 = PI * float(i + 1) / stacks;
        for (int j = 0; j < slices; j++) {
            float ph0 = 2.0f * PI * float(j) / slices;
            float ph1 = 2.0f * PI * float(j + 1) / slices;
            Vec3 a = sph(ph0, th0), b = sph(ph1, th0);
            Vec3 c = sph(ph0, th1), d = sph(ph1, th1);
            emit(a);
            emit(b);
            emit(c);
            emit(b);
            emit(d);
            emit(c);
        }
    }
    root_.children.push_back(std::move(node));
    markDirty();
}

// ── 水平地面 (法线 +Y) ──
void G3D::addPlane(float w, float d, float tx, float ty, float tz) {
    G3DNode node;
    float hw = w * 0.5f, hd = d * 0.5f;
    node.vertices.push_back({-hw, 0, -hd, 0, 1, 0});
    node.vertices.push_back({hw, 0, -hd, 0, 1, 0});
    node.vertices.push_back({-hw, 0, hd, 0, 1, 0});
    node.vertices.push_back({hw, 0, -hd, 0, 1, 0});
    node.vertices.push_back({hw, 0, hd, 0, 1, 0});
    node.vertices.push_back({-hw, 0, hd, 0, 1, 0});
    root_.children.push_back(std::move(node));
    markDirty();
}

// ============================================================================
// getProperty / setProperty
// ============================================================================
std::string G3D::getProperty(const char *name) const {
    if (std::strcmp(name, "autoRotate") == 0) return autoRotate_ ? "true" : "false";
    if (std::strcmp(name, "showAxes") == 0) return showAxes_ ? "true" : "false";    // ← 新增
    return View::getProperty(name);
}

// ============================================================================
// setPropertyTyped — 属性写入唯一入口（autoRotate/showAxes）
// ============================================================================
bool G3D::setPropertyTyped(const char *name, const TypedProp &value) {
	if (std::strcmp(name, "autoRotate") == 0) {
		auto b = typedToBool(value);      // 兼容 "true"/"1"
		if (!b) { return false; }
		setAutoRotate(*b);
		return true;
	}
	if (std::strcmp(name, "showAxes") == 0) {
		auto b = typedToBool(value);
		if (!b) { return false; }
		setShowAxes(*b);
		return true;
	}
	return View::setPropertyTyped(name, value);
}