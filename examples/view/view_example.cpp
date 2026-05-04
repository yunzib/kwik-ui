/**
 * @file main.cpp
 * @brief KwiK UI 应用程序入口
 */
#if defined(_WIN32)
#include <Windows.h>
#endif
#include <iostream>
#include <string>

import kwik.platform;
import kwik.engine.context;

import std;



int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);      // 设置控制台输出为 UTF-8
    SetConsoleCP(CP_UTF8);            // 设置控制台输入为 UTF-8
#endif
    
    // ==================== 3. 初始化JS上下文引擎====================
    QuickJSContext jsContext{};

    // 加载执行 js 文件
    jsContext.evalFile("../../examples/view/view.js");
   
    // 解析ui树
    
    
    
    std::println("exit........"); 
    
    // ==================== 8. 清理资源 ====================
    // renderThread 智能指针会自动销毁并停止渲染线程
    return 0;
}