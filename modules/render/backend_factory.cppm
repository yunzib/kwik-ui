// ============================================================================
// 模块: kwik.render.backend_factory
// 渲染后端选择工厂（清单 §四）：具体后端类型与创建集中于此，
// 渲染线程只面向 RenderBackend 抽象（不再 import 具体后端模块）。
// ============================================================================
module;
#include <memory>

export module kwik.render.backend_factory;

import kwik.render.backend;

import std;

/** @brief 按类型创建渲染后端。Software/OpenGL/Metal 尚未实现（随 §十四
 *         多呈现目标落地），返回 nullptr 由调用方报错。 */
export std::unique_ptr<RenderBackend> createBackend(BackendType type);
