// ============================================================================
// crash_reporter.cppm — 进程级崩溃报告
//
// 未处理异常 → minidump 落盘（crashes/ 目录，时间戳命名）。发版后崩溃
// 仅剩日志断片的排障困境的最小解；进程级安装一次（Application::run）。
// win32 专属实现（SEH + MiniDumpWriteDump）；其他平台为空实现占位
// （POSIX 信号 + coredump 方案随平台层立项）。
// ============================================================================
export module kwik.app.crash_reporter;

/**
 * @brief 安装未处理异常过滤器（进程级，幂等）
 *
 * 受控自检：环境变量 KWIK_CRASH_TEST=1 时安装后延迟触发空指针写，
 * 验证转储落盘链路（仅用于手工/脚本验证）。
 */
export void install_crash_reporter();
