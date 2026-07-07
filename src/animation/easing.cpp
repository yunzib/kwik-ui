module;
#include <cmath>
#include <charconv>

module kwik.animation.easing;
import kwik.core.log;

import std;

EasingConfig parseEasing(const std::string &desc) {
    EasingConfig cfg;
    if (desc == "linear")
        cfg.type = EasingConfig::Linear;
    else if (desc == "ease")
        cfg.type = EasingConfig::Ease;
    else if (desc == "easeIn")
        cfg.type = EasingConfig::EaseIn;
    else if (desc == "easeOut")
        cfg.type = EasingConfig::EaseOut;
    else if (desc == "easeInOut")
        cfg.type = EasingConfig::EaseInOut;
    else if (desc.starts_with("spring(")) {
        cfg.type = EasingConfig::Spring;
        // "spring(100,10)" → stiffness=100, damping=10
        auto start = desc.find('(') + 1;
        auto comma = desc.find(',', start);
        if (comma != std::string::npos) {
            auto [p, ec1] = std::from_chars(desc.data() + start, desc.data() + comma, cfg.stiffness);
            auto end = desc.find(')', comma);
            if (end != std::string::npos) {
                auto [p2, ec2] = std::from_chars(desc.data() + comma + 1, desc.data() + end, cfg.damping);
                if (ec1 != std::errc{} || ec2 != std::errc{}) {
                    Log::warn("[easing] 解析 spring 参数失败: {}", desc);
                    cfg.type = EasingConfig::Ease;   // 解析失败回退到 ease
                    cfg.stiffness = 100.0f;
                    cfg.damping = 10.0f;
                }
            }
        }
    }
    // 否则保持默认 cfg.type = Ease
    return cfg;
}

static float applyBezier(float t, float p1x, float p1y, float p2x, float p2y) {
    // 使用 Newton-Raphson 求 t 对应的 x 位置上的 y 值
    float u = t;
    for (int i = 0; i < 8; ++i) {
        float mt = 1.0f - u;
        float x = 3.0f * u * mt * mt * p1x + 3.0f * u * u * mt * p2x + u * u * u;
        float dx = 3.0f * mt * mt * p1x + 6.0f * u * mt * (p2x - p1x) + 3.0f * u * u * (1.0f - p2x);
        if (std::abs(dx) < 1e-6f) break;
        u -= (x - t) / dx;
    }
    u = std::clamp(u, 0.0f, 1.0f);
    float mt = 1.0f - u;
    return 3.0f * u * mt * mt * p1y + 3.0f * u * u * mt * p2y + u * u * u;
}

float applyEasing(float t, const EasingConfig &cfg) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (cfg.type) {
    case EasingConfig::Linear: return t;
    case EasingConfig::Ease: return applyBezier(t, 0.25f, 0.1f, 0.25f, 1.0f);
    case EasingConfig::EaseIn: return applyBezier(t, 0.42f, 0.0f, 1.0f, 1.0f);
    case EasingConfig::EaseOut: return applyBezier(t, 0.0f, 0.0f, 0.58f, 1.0f);
    case EasingConfig::EaseInOut: return applyBezier(t, 0.42f, 0.0f, 0.58f, 1.0f);
    case EasingConfig::CubicBezier: return applyBezier(t, cfg.p1x, cfg.p1y, cfg.p2x, cfg.p2y);
    case EasingConfig::Spring: {
        // 弹簧: 过阻尼二阶系统
        float s = cfg.stiffness;
        float d = cfg.damping;
        float beta = d / (2.0f * std::sqrt(s));
        if (beta < 1.0f) {
            // 欠阻尼: 振荡衰减
            float w = std::sqrt(s * (1.0f - beta * beta));
            float phi = std::atan2(w, beta * std::sqrt(s));
            return 1.0f - std::exp(-beta * std::sqrt(s) * t) * std::sin(w * t + phi) / std::sin(phi);
        }
        // 临界/过阻尼
        float st = std::sqrt(s) * t;
        return 1.0f - std::exp(-d / 2.0f * t) * (1.0f + st);
    }
    }
    return t;
}