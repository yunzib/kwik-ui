/**
 * @file main.cpp
 * @brief KwiK UI 应用程序入口
 */
#if defined(_WIN32)
#include <Windows.h>
#endif
#include <iostream>
#include <string>
import kwik.core;
import kwik.platform;
import kwik.render;
import kwik.element;
import kwik.engine;
import std;
// 默认UI脚本路径
constexpr const char* DEFAULT_UI_SCRIPT = "examples/view/view.js";
// 帮助信息
void printUsage(const char* programName) {
    std::cout << "用法: " << programName << " [脚本路径] [后端类型]\n";
    std::cout << "后端类型:\n";
    std::cout << "  vulkan   - Vulkan渲染后端 (默认)\n";
    std::cout << "  opengl   - OpenGL渲染后端\n";
    std::cout << "  software - 软件渲染后端 (CPU)\n";
    std::cout << "示例:\n";
    std::cout << "  " << programName << " examples/view/view.js vulkan\n";
    std::cout << "  " << programName << " examples/view/view.js opengl\n";
}
// 解析后端类型
kwik::render::BackendType parseBackendType(const std::string& typeStr) {
    if (typeStr == "opengl") {
        return kwik::render::BackendType::OpenGL;
    } else if (typeStr == "software") {
        return kwik::render::BackendType::Software;
    }
    // 默认使用Vulkan
    return kwik::render::BackendType::Vulkan;
}


int main(int argc, char* argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    // ==================== 解析命令行参数 ====================
    std::string uiScriptPath = DEFAULT_UI_SCRIPT;
    kwik::render::BackendType backendType = kwik::render::BackendType::Vulkan;
    
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        uiScriptPath = arg1;
    }
    
    if (argc > 2) {
        backendType = parseBackendType(argv[2]);
    }
    
    std::cout << "KwiK UI 启动配置:\n";
    std::cout << "  脚本路径: " << uiScriptPath << "\n";
    std::cout << "  渲染后端: ";
    switch (backendType) {
        case kwik::render::BackendType::Vulkan:
            std::cout << "Vulkan\n";
            break;
        case kwik::render::BackendType::OpenGL:
            std::cout << "OpenGL\n";
            break;
        case kwik::render::BackendType::Software:
            std::cout << "Software (CPU)\n";
            break;
        default:
            std::cout << "未知\n";
    }
    
    // ==================== 1. 创建窗口 ====================
    auto window = platform::CreatePlatformWindow();
    
    if (!window->Create("KwiK UI - 多后端渲染演示", 800, 600)) {
        std::cerr << "错误: 创建窗口失败" << std::endl;
        return -1;
    }
    
    window->Show();
    
    // ==================== 2. 初始化渲染线程 ====================
    std::unique_ptr<kwik::render::RenderThread> renderThread;
    try {
        kwik::render::RenderThreadConfig config;
        config.backendType = backendType;
        config.initialWidth = 800;
        config.initialHeight = 600;
        config.callbacks.onResize = [](int width, int height) {
            std::cout << "窗口大小改变: " << width << "x" << height << std::endl;
        };
        config.callbacks.onError = [](const std::string& error) {
            std::cerr << "渲染线程错误: " << error << std::endl;
        };
        config.callbacks.onStarted = []() {
            std::cout << "渲染线程启动成功" << std::endl;
        };
        config.callbacks.onStopped = []() {
            std::cout << "渲染线程已停止" << std::endl;
        };
        
        renderThread = std::make_unique<kwik::render::RenderThread>(*window, config);
        
        if (!renderThread->start()) {
            throw std::runtime_error("无法启动渲染线程");
        }
        
        if (!renderThread->waitForRunning(5000)) {
            throw std::runtime_error("渲染线程启动超时");
        }
        
        std::cout << "渲染线程初始化成功: ";
        switch (renderThread->backendType()) {
            case kwik::render::BackendType::Vulkan:
                std::cout << "Vulkan\n";
                break;
            case kwik::render::BackendType::OpenGL:
                std::cout << "OpenGL\n";
                break;
            case kwik::render::BackendType::Software:
                std::cout << "Software (CPU)\n";
                break;
            default:
                std::cout << "未知\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "错误: 渲染线程初始化失败 - " << e.what() << std::endl;
        return -1;
    }
    
    // ==================== 3. 初始化JS引擎 ====================
    auto runtime = engine::QuickJSRuntime::getInstance();
    engine::QuickJSContext jsContext;
    
    // 注册View函数
    element::ViewFactory::registerViewFunction(jsContext.getPtr());
    
    // ==================== 4. 加载JS脚本 ====================
    std::string scriptPath = engine::ScriptLoader::resolvePath(uiScriptPath);
    
    std::cout << "加载UI脚本: " << scriptPath << std::endl;
    
    auto scriptContent = engine::ScriptLoader::loadFromFile(scriptPath);
    
    if (!scriptContent.has_value()) {
        std::cerr << "错误: 加载脚本失败: " << scriptPath << std::endl;
        delete renderContext;
        return -1;
    }
    
    // 执行脚本
    JSValue result = JS_Eval(
        jsContext.getPtr(), 
        scriptContent->c_str(), 
        scriptContent->size(), 
        scriptPath.c_str(), 
        JS_EVAL_TYPE_GLOBAL
    );
    
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(jsContext.getPtr());
        const char* errStr = JS_ToCString(jsContext.getPtr(), exception);
        std::cerr << "JS执行错误: " << (errStr ? errStr : "未知错误") << std::endl;
        JS_FreeCString(jsContext.getPtr(), errStr);
        JS_FreeValue(jsContext.getPtr(), exception);
        JS_FreeValue(jsContext.getPtr(), result);
        // renderThread will be automatically destroyed
        return -1;
    }
    
    // ==================== 5. 创建View树 ====================
    auto rootView = element::ViewFactory::createView(jsContext.getPtr(), result);
    JS_FreeValue(jsContext.getPtr(), result);
    
    if (!rootView) {
        std::cerr << "错误: 创建View失败" << std::endl;
        // renderThread will be automatically destroyed
        return -1;
    }
    
    // ==================== 6. 布局 ====================
    int winWidth, winHeight;
    window->GetSize(&winWidth, &winHeight);
    
    Constraints rootConstraints = Constraints::fixed(
        static_cast<float>(winWidth),
        static_cast<float>(winHeight)
    );
    
    Size rootSize = rootView->measure(rootConstraints);
    rootView->layout(Rect(0, 0, rootSize.width, rootSize.height));
    
    // ==================== 7. 渲染循环 ====================
    bool running = true;
    int frameCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    window->SetEventCallback([&running, &renderThread](const platform::Event& e) {
        if (e.type == platform::Event::Type::WindowClose) {
            running = false;
        }
        // 将窗口事件转发到渲染线程
        if (renderThread) {
            renderThread->submitWindowEvent(e);
        }
    });
    
    std::cout << "\n进入渲染循环... (按ESC或关闭窗口退出)\n";
    
    while (running && window) {
        window->PollEvents();
        
        // 获取当前帧的命令缓冲区
        auto& cmdBuffer = renderThread->commandQueue().currentBuffer();
        
        // 创建Graphics对象并绑定到命令缓冲区
        kwik::render::Graphics canvas(&cmdBuffer);
        
        // 开始帧（记录命令）
        canvas.beginFrame();
        
        // 清除背景
        canvas.clear(Color::white());
        
        // 绘制View树
        rootView->draw(canvas);
        
        // 结束帧
        canvas.endFrame();
        
        // 提交命令缓冲区到渲染线程
        if (!renderThread->commandQueue().submit()) {
            std::cerr << "警告: 命令队列已满，丢帧" << std::endl;
        }
        
        // 显示FPS信息
        frameCount++;
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - startTime).count();
        
        if (elapsed >= 1000) {
            float fps = frameCount * 1000.0f / elapsed;
            std::cout << "FPS: " << fps << " (后端: ";
            switch (renderThread->backendType()) {
                case kwik::render::BackendType::Vulkan:
                    std::cout << "Vulkan";
                    break;
                case kwik::render::BackendType::OpenGL:
                    std::cout << "OpenGL";
                    break;
                case kwik::render::BackendType::Software:
                    std::cout << "Software";
                    break;
                default:
                    std::cout << "未知";
            }
            std::cout << ")\r";
            std::cout.flush();
            
            frameCount = 0;
            startTime = currentTime;
        }
    }
    
    std::cout << "\n程序正常退出\n";
    
    // ==================== 8. 清理资源 ====================
    // renderThread 智能指针会自动销毁并停止渲染线程
    return 0;
}