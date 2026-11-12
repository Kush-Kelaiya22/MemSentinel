#!/usr/bin/env python3
"""Benchmark stock GCC/G++ compilation time vs. Safe-C plugin overhead."""

from __future__ import annotations

import argparse
import os
import platform
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def generate_heavy_c(path: Path) -> None:
    """Create a realistic ~120-function C source file with pointers and branches."""
    lines = ["#include <stdlib.h>", "#include <string.h>", ""]
    for idx in range(120):
        lines.extend([
            f"int proc_{idx}(int n, const int *src) {{",
            "    int acc = 0;",
            "    int *buf = (int *)malloc(16 * sizeof(int));",
            "    if (!buf) return -1;",
            "    for (int i = 0; i < 16; ++i) {",
            f"        buf[i] = (i + {idx}) * (n & 7);",
            "        if (src && (i & 1)) acc += src[0];",
            "    }",
            "    for (int i = 0; i < 16; ++i) acc += buf[i];",
            "    free(buf);",
            "    return acc;",
            "}",
            "",
        ])
    path.write_text("\n".join(lines), encoding="utf-8")


def generate_stl_heavy_cpp(path: Path) -> None:
    """Create a multi-container C++20 source using vector, string, map, unordered_map."""
    path.write_text(
        """#include <algorithm>
#include <map>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

struct Record {
    std::string label;
    std::vector<int> values;
};

int run_pipeline(int seed) {
    std::vector<Record> items;
    items.reserve(16);
    for (int i = 0; i < 8; ++i) {
        items.push_back({"k" + std::to_string(i + seed), {i, i + 1, i + 2}});
    }

    std::map<std::string, int> ordered;
    std::unordered_map<std::string, int> hashed;
    for (const auto& rec : items) {
        int sum = std::accumulate(rec.values.begin(), rec.values.end(), 0);
        ordered[rec.label] = sum;
        hashed[rec.label] = sum * 2;
    }

    std::vector<std::string> keys;
    keys.reserve(ordered.size());
    for (const auto& [k, _] : ordered) {
        keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());

    int total = 0;
    for (const auto& k : keys) {
        auto it = hashed.find(k);
        if (it != hashed.end()) {
            total += it->second;
        }
    }
    return total;
}
""",
        encoding="utf-8",
    )


def time_command(cmd: list[str], warmup: int, runs: int) -> list[float]:
    for _ in range(warmup):
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    samples_ms: list[float] = []
    for _ in range(runs):
        t0 = time.perf_counter()
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        samples_ms.append((time.perf_counter() - t0) * 1000.0)
    return samples_ms


def cpu_info() -> str:
    try:
        for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine()


def gcc_version(compiler: str) -> str:
    res = subprocess.run([compiler, "--version"], capture_output=True, text=True, check=True)
    return res.stdout.splitlines()[0].strip()


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark stock GCC/G++ vs. Safe-C compile time.")
    parser.add_argument("--runs", type=int, default=7, help="Measured runs per configuration (default: 7)")
    parser.add_argument("--warmup", type=int, default=2, help="Warmup runs before measurement (default: 2)")
    parser.add_argument("--plugin", default="build/lib/safe_c.so", help="Path to safe_c.so")
    parser.add_argument("--specs", default="src/specs/safe_c.specs", help="Path to safe_c.specs")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    os.chdir(repo_root)

    plugin_path = Path(args.plugin).resolve()
    specs_path = Path(args.specs).resolve()
    if not plugin_path.exists():
        print(f"Error: plugin not found at {plugin_path}", file=sys.stderr)
        return 1

    print(f"Compiler : {gcc_version('g++')}")
    print(f"CPU      : {cpu_info()} ({os.cpu_count()} logical cores)")
    print(f"OS       : {platform.system()} {platform.release()} ({platform.machine()})")
    print(f"Runs     : {args.warmup} warmup + {args.runs} measured per variant\n")

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        heavy_c = tmp / "heavy_c.c"
        heavy_stl = tmp / "heavy_stl.cpp"
        out_obj = tmp / "out.o"
        generate_heavy_c(heavy_c)
        generate_stl_heavy_cpp(heavy_stl)

        scenarios = [
            ("C small (-O0)", "gcc", ["-O0", "-c", "tests/compile_pass/valid_edge_cases.c"], []),
            ("C 120-func heap/loop (-O0)", "gcc", ["-O0", "-c", str(heavy_c)], []),
            ("C 120-func heap/loop (-O2)", "gcc", ["-O2", "-c", str(heavy_c)], []),
            ("C++ STL patterns (-O0)", "g++", ["-O0", "-std=c++20", "-c", "tests/compile_pass/valid_stl_patterns.cpp"], []),
            ("C++ STL patterns (-O2)", "g++", ["-O2", "-std=c++20", "-c", "tests/compile_pass/valid_stl_patterns.cpp"], []),
            ("C++ multi-container STL (-O2)", "g++", ["-O2", "-std=c++20", "-c", str(heavy_stl)], []),
            ("C++20 real TU analyzer.cpp (-O0)", "g++", ["-O0", "-std=c++20", "-Iinclude", "-c", "src/core/analyzer.cpp"], []),
            ("C++20 real TU analyzer.cpp (-O2)", "g++", ["-O2", "-std=c++20", "-Iinclude", "-c", "src/core/analyzer.cpp"], []),
            ("C++20 analyzer.cpp bypass (disable flag)", "g++", ["-O2", "-std=c++20", "-Iinclude", "-c", "src/core/analyzer.cpp"], ["-fplugin-arg-safe_c-disable"]),
        ]

        header = f"{'Scenario':42} | {'Stock (ms)':>10} | {'Safe-C (ms)':>11} | {'Delta':>18}"
        print(header)
        print("-" * len(header))

        for name, cc, flags, extra_plug_flags in scenarios:
            base_cmd = [cc, *flags, "-o", str(out_obj)]
            plug_cmd = [cc, f"-specs={specs_path}", f"-fplugin={plugin_path}", *extra_plug_flags, *flags, "-o", str(out_obj)]

            base_samples = time_command(base_cmd, args.warmup, args.runs)
            plug_samples = time_command(plug_cmd, args.warmup, args.runs)

            base_med = statistics.median(base_samples)
            plug_med = statistics.median(plug_samples)
            delta = plug_med - base_med
            pct = (delta / base_med * 100.0) if base_med > 0 else 0.0
            print(f"{name:42} | {base_med:10.1f} | {plug_med:11.1f} | {delta:+7.1f} ms ({pct:+5.1f}%)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
