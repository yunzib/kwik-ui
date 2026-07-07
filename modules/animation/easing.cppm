module;

export module kwik.animation.easing;

import kwik.core.types;
import std;

// EasingConfig 定义已迁移至 kwik.core.types，此处仅声明算法函数

/**
 * @brief 解析 easing 描述字符串
 *  "linear" | "ease" | "easeIn" | "easeOut" | "easeInOut"
 *  "spring(100,10)" | "spring(200,20)"
 *  不支持 cubic-bezier 字符串（仅从数组解析）
 */
export EasingConfig parseEasing(const std::string &desc);

/**
 * @brief 应用缓动函数
 * @param t  输入进度 [0, 1]
 * @param cfg 缓动配置
 * @return 缓动后的输出值 (弹簧可能超出 [0,1] 范围)
 */
export float applyEasing(float t, const EasingConfig &cfg);