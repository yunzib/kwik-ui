module;

#include "quickjs.h"
#include <memory>

export module kwik.engine.runtime;

import std;

export class QuickJSRuntime {
    public:
         ~QuickJSRuntime();         // shared_ptr需要访问析构函数
        static std::shared_ptr<QuickJSRuntime> getInstance();
        JSRuntime* getPtr() const {return runtime;}

    private:
        QuickJSRuntime();
       

        JSRuntime* runtime;
};
