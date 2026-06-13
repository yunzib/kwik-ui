module;

#include "quickjs.h"

module kwik.engine.runtime;

    QuickJSRuntime::QuickJSRuntime() {
        runtime = JS_NewRuntime();
        JS_SetMemoryLimit(runtime, 16 * 1024 * 1024);  // 16MB 上限，防止无限制膨胀
    }

    QuickJSRuntime::~QuickJSRuntime() {
        JS_FreeRuntime(runtime);
    }

    std::shared_ptr<QuickJSRuntime> QuickJSRuntime::getInstance() {
        static std::weak_ptr<QuickJSRuntime> weak;
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        auto ptr = weak.lock();
        if (!ptr) {
            ptr = std::shared_ptr<QuickJSRuntime>(new QuickJSRuntime());
            weak = ptr;
        }
        return ptr;
    }
