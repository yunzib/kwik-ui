#!/usr/bin/env python3
"""冒烟测试：遍历示例 → N 帧自动退出 → 扫描错误 → 报告

用法（在 build/test 目录）:
  python ../../tools/smoke.py            # 跑默认示例集
  python ../../tools/smoke.py a b c      # 只跑指定示例
  python ../../tools/smoke.py --frames 400 --validation

判定: 进程退出码(KWIK_SMOKE_FRAMES 模式下 = Log::error 计数) + 输出扫描
([Error]/failed: -4/Assertion/DEVICE_LOST)
"""
import os
import re
import subprocess
import sys
import time

# 默认示例集（含资源/交互依赖的可按需增删）
DEMOS = [
    "view", "gradient", "text", "button", "flex", "grid", "stack", "image",
    "input", "textarea", "dropdown", "radiobutton", "checkbox", "slider",
    "progress", "switch", "line", "spinner", "tabs", "table", "textview",
    "theme", "layer", "animation", "transition", "rotate", "scrollview",
    "lazylist", "gauge", "progressring", "spinbox", "glass",
    "chart", "datepicker", "car", "g2d", "example",
]
# 已知不稳定(退出段错误,非确定性): image —— 退出竞态待查(可用 llvm-mingw 的 lldb 取栈),
# 修复前不进默认集,详见 docs/当前优化任务清单.md §七
ERROR_PAT = re.compile(r"\[Error\]|failed: -4|Assertion|DEVICE_LOST")
ANSI_PAT = re.compile(r"\[[0-9;]*m")


def run_demo(demo: str, frames: int, validation: bool, exe: str):
    env = dict(os.environ)
    env["KWIK_SMOKE_FRAMES"] = str(frames)
    env["KWIK_VALIDATION"] = "1" if validation else "0"
    t0 = time.time()
    try:
        p = subprocess.run([exe, demo], capture_output=True, text=True,
                           timeout=frames // 20 + 30, env=env, errors="replace")
        rc, out = p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired as e:
        # text=True 时 e.stdout 为 str（或 None），与 bytes 分支统一为 str
        out = e.stdout if isinstance(e.stdout, str) else (e.stdout or b"").decode(errors="replace")
        return "TIMEOUT", out, time.time() - t0
    hits = [l.strip() for l in out.splitlines() if ERROR_PAT.search(ANSI_PAT.sub("", l))]
    status = "PASS" if rc == 0 and not hits else "FAIL"
    return status, hits, time.time() - t0


def main():
    argv = sys.argv[1:]
    frames, validation = 40, False
    if "--frames" in argv:
        i = argv.index("--frames")
        frames = int(argv[i + 1])
        del argv[i:i + 2]
    validation = "--validation" in argv
    args = [a for a in argv if not a.startswith("--")]
    demos = args if args else DEMOS

    exe = "./example.exe" if os.name == "nt" else "./example"
    results, failed = [], 0
    for demo in demos:
        status, detail, secs = run_demo(demo, frames, validation, exe)
        if status != "PASS":
            failed += 1
        results.append((demo, status, secs, detail))
        print(f"{demo:<12} {status:<8} {secs:5.1f}s  " + ("; ".join(detail[:2])[:120] if detail else ""), flush=True)

    print(f"\n===== smoke: {len(demos) - failed}/{len(demos)} passed =====")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
