/**
 * @file ring_geom.cppm
 * @brief SDF 圆环共享几何（纯数学，无渲染依赖）
 *
 * 圆环形组件（内置 ProgressRing / 扩展 Chart gauge）的几何口径统一：
 *   - 中心与半径求解（内容框 → 圆心 + 内切半径）
 *   - 带宽两口径换算：ratio（带宽占 r 比例，gauge 系）与
 *     thickness（像素带宽，ProgressRing 系）
 *   - 值 → 弧度角线性映射（度数起角/扫角）
 *
 * 依赖方向：core ← 扩展（chart 扩展与内置组件都 import 本模块，
 * 扩展不能反向被 core 依赖，故几何下沉于此）。
 */
export module kwik.core.ring_geom;

import std;

export {

/** @brief 圆环带几何（fillRing 参数口径：中径 + 半带宽）。 */
struct RingBand {
	float midR = 0.0f;    ///< 带中径（外缘与内缘的均值）
	float halfW = 0.0f;   ///< 半带宽（带宽的一半）
};

/** @brief 内容框中心与内切半径。 */
struct RingCenter {
	float cx = 0.0f;    ///< 圆心 x
	float cy = 0.0f;    ///< 圆心 y
	float r = 0.0f;     ///< 内切半径（min(宽,高)/2）
};

/**
 * @brief 内容框 → 圆心 + 内切半径
 * @param l/t/w/h 内容框（已扣 padding）
 */
inline RingCenter ringCenterOf(float l, float t, float w, float h) {
	RingCenter c;
	c.cx = l + w * 0.5f;
	c.cy = t + h * 0.5f;
	c.r = std::min(w, h) * 0.5f;
	return c;
}

/**
 * @brief 比例带宽（gauge 系口径）：带宽 = r * ratio，外缘固定 r * outerRatio 向内收
 * @param r          内切半径（基准）
 * @param ratio      带宽占 r 比例（0 = 无带）
 * @param outerRatio 外缘占 r 比例（gauge 固定 0.95）
 */
inline RingBand ringBandByRatio(float r, float ratio, float outerRatio = 0.95f) {
	RingBand b;
	float w = r * ratio;
	b.midR = r * outerRatio - w * 0.5f;
	b.halfW = w * 0.5f;
	return b;
}

/**
 * @brief 像素带宽（ProgressRing 系口径）：外缘贴近 r 留 inset 像素边距，带宽 thickness 像素
 * @param r          内切半径（基准）
 * @param thickness  带宽（像素）
 * @param inset      外缘距 r 的边距（ProgressRing 固定 2）
 */
inline RingBand ringBandByThickness(float r, float thickness, float inset = 2.0f) {
	RingBand b;
	b.midR = r - inset - thickness * 0.5f;
	b.halfW = thickness * 0.5f;
	return b;
}

/**
 * @brief 值 → 弧度角（线性映射）
 * @param value     当前值（不裁剪，调用方自行 clamp）
 * @param minV/maxV 值域
 * @param startDeg  起角（度）
 * @param sweepDeg  扫角（度，正 = 顺时针方向推进）
 * @return 弧度角（Graphics::fillRing 口径）
 */
inline float ringAngleOf(float value, float minV, float maxV, float startDeg, float sweepDeg) {
	float range = maxV - minV;
	if (range <= 0.0f) range = 1.0f;    // 防御：值域非法退化为常角
	return (startDeg + sweepDeg * (value - minV) / range) * (std::acos(-1.0f) / 180.0f);
}

}  // export
