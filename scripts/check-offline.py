#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run disposable-file and optional desktop smoke tests with process network sockets denied.

The filter is inherited by the tested process. Local Unix sockets remain available
for X11 and D-Bus. This is not a network namespace: separately running operating
system services are outside the filter. No image-writing/device commands run.
Exit 77 means this Linux host lacks the required test capability.
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import errno
import json
import math
import os
from pathlib import Path
import platform
import resource
import shutil
import socket
import subprocess
import sys
import tempfile
import time


class Unsupported(RuntimeError):
    pass


class ArgumentComparison(ctypes.Structure):
    _fields_ = [("arg", ctypes.c_uint), ("op", ctypes.c_int),
                ("datum_a", ctypes.c_uint64), ("datum_b", ctypes.c_uint64)]


def deny_network() -> None:
    """Install and verify an inherited seccomp guard, without host configuration changes."""
    library = ctypes.util.find_library("seccomp")
    if sys.platform != "linux" or not library:
        raise Unsupported("Linux libseccomp is required for the network-denied test.")
    seccomp = ctypes.CDLL(library, use_errno=True)
    seccomp.seccomp_init.argtypes = [ctypes.c_uint32]
    seccomp.seccomp_init.restype = ctypes.c_void_p
    seccomp.seccomp_syscall_resolve_name.argtypes = [ctypes.c_char_p]
    seccomp.seccomp_syscall_resolve_name.restype = ctypes.c_int
    seccomp.seccomp_rule_add_array.argtypes = [ctypes.c_void_p, ctypes.c_uint32,
        ctypes.c_int, ctypes.c_uint, ctypes.POINTER(ArgumentComparison)]
    seccomp.seccomp_load.argtypes = [ctypes.c_void_p]
    seccomp.seccomp_release.argtypes = [ctypes.c_void_p]
    context = seccomp.seccomp_init(0x7FFF0000)  # SCMP_ACT_ALLOW
    if not context:
        raise Unsupported("Cannot allocate a seccomp filter.")
    try:
        comparison = ArgumentComparison(0, 1, socket.AF_UNIX, 0)  # SCMP_CMP_NE
        for name in (b"socket", b"socketpair", b"io_uring_setup"):
            number = seccomp.seccomp_syscall_resolve_name(name)
            if number < 0:
                if name == b"io_uring_setup":
                    continue
                raise Unsupported(f"Cannot resolve required syscall {name.decode()}.")
            arguments = 0 if name == b"io_uring_setup" else 1
            result = seccomp.seccomp_rule_add_array(context, 0x00050000 | errno.EPERM,
                number, arguments, None if not arguments else ctypes.byref(comparison))
            if result:
                raise Unsupported(f"Cannot prepare seccomp rule: errno {-result}.")
        result = seccomp.seccomp_load(context)
        if result:
            raise Unsupported(f"Cannot load seccomp filter: errno {-result}.")
    finally:
        seccomp.seccomp_release(context)

    for family in (socket.AF_INET, socket.AF_INET6):
        try:
            probe = socket.socket(family, socket.SOCK_STREAM)
        except OSError as error:
            if error.errno != errno.EPERM:
                raise RuntimeError("The network-denial probe returned an unexpected error.") from error
        else:
            probe.close()
            raise RuntimeError("The network-denial probe unexpectedly opened an Internet socket.")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM):
        pass


def restricted_child(output: Path, command: list[str]) -> int:
    try:
        deny_network()
        started = time.monotonic()
        result = subprocess.run(command, text=True, capture_output=True, timeout=60,
                                close_fds=True)
        record = {"command": Path(command[0]).name, "exit_code": result.returncode,
                  "seconds": round(time.monotonic() - started, 4),
                  "peak_rss_kib": resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,
                  "guard_verified": True, "stdout": result.stdout, "stderr": result.stderr}
        output.write_text(json.dumps(record), encoding="utf-8")
        return 0
    except Unsupported as error:
        print(f"Offline test unavailable: {error}", file=sys.stderr)
        return 77


def run_guarded(command: list[str], folder: Path, desktop: bool) -> dict:
    record = folder / ("desktop.json" if desktop else "cli.json")
    child = [sys.executable, str(Path(__file__).resolve()), "--restricted-child", str(record), *command]
    environment = os.environ.copy()
    for name in ("XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME"):
        directory = folder / name.lower()
        directory.mkdir(mode=0o700)
        environment[name] = str(directory)
    # Isolate from any already running Kavrune instance and the user's desktop.
    environment.update(GTK_A11Y="none", GSETTINGS_BACKEND="memory", GDK_BACKEND="x11")
    if desktop:
        wrappers = [shutil.which(name) for name in ("dbus-run-session", "xvfb-run", "Xvfb")]
        if not all(wrappers):
            raise Unsupported("The desktop test requires dbus-run-session, xvfb-run and Xvfb.")
        child = [wrappers[0], "--", wrappers[1], "-a", *child]
    completed = subprocess.run(child, env=environment, text=True, capture_output=True, timeout=80)
    if completed.returncode == 77:
        raise Unsupported(completed.stderr.strip() or "The seccomp test capability is unavailable.")
    if completed.returncode:
        raise RuntimeError(f"The guarded test runner failed: {completed.stderr.strip()}")
    result = json.loads(record.read_text(encoding="utf-8"))
    if result["exit_code"]:
        raise RuntimeError(f"{result['command']} failed under the network guard: {result['stderr'].strip()}")
    return result


def check(cli: Path, desktop: Path | None, report: Path | None,
          seconds_budget: float, memory_budget_mib: int) -> None:
    if os.geteuid() == 0:
        raise Unsupported("Run the offline test as an ordinary user; Kavrune rejects root operation.")
    with tempfile.TemporaryDirectory(prefix="kavrune-offline-") as directory:
        folder = Path(directory)
        cli_folder = folder / "cli"
        cli_folder.mkdir(mode=0o700)
        results = [run_guarded([str(cli), "self-test", "--json"], cli_folder, False)]
        payload = json.loads(results[0]["stdout"])
        if payload.get("passed") is not True or payload.get("physical_usb_tested") is not False:
            raise ValueError("The guarded CLI disposable-file self-test did not pass.")
        if desktop:
            desktop_folder = folder / "desktop"
            desktop_folder.mkdir(mode=0o700)
            results.append(run_guarded([str(desktop), "--smoke-test"], desktop_folder, True))
        for result in results:
            if result["seconds"] > seconds_budget:
                raise ValueError(f"{result['command']} exceeded the {seconds_budget:g}s smoke-test budget.")
            if result["peak_rss_kib"] > memory_budget_mib * 1024:
                raise ValueError(f"{result['command']} exceeded the {memory_budget_mib}MiB peak-RSS budget.")
            # Output can contain local D-Bus addresses; retain measurements, not raw diagnostics.
            result.pop("stdout")
            result.pop("stderr")
        summary = {"schema_version": 1, "passed": True, "platform": platform.system(),
            "architecture": platform.machine(),
            "scope": "Process-inherited seccomp denial of non-AF_UNIX sockets and io_uring_setup; "
                     "existing OS services are outside the guard. Disposable-file CLI test; optional "
                     "private-X11 desktop startup with read-only UDisks metadata discovery. "
                     "No physical drive read/write/format operation; no package-manager installation.",
            "performance_scope": "Host smoke-test baseline, not cold-start, idle-CPU, interaction "
                                 "latency or hardware qualification. Desktop deliberately waits 2s before exiting.",
            "budgets": {"seconds_per_test": seconds_budget, "peak_rss_mib": memory_budget_mib},
            "tests": results}
        if report:
            report.parent.mkdir(parents=True, exist_ok=True)
            report.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(summary, indent=2))


def main() -> int:
    if len(sys.argv) >= 5 and sys.argv[1] == "--restricted-child":
        return restricted_child(Path(sys.argv[2]), sys.argv[3:])
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--desktop", type=Path, help="also test desktop in a private X11/D-Bus session")
    parser.add_argument("--report", type=Path, help="write measurements and exact evidence scope as JSON")
    parser.add_argument("--seconds-budget", type=float, default=10,
                        help="per-command smoke duration budget, including desktop's deliberate 2s wait")
    parser.add_argument("--memory-budget-mib", type=int, default=512,
                        help="provisional host smoke peak-RSS limit; not a measured idle-memory target")
    args = parser.parse_args()
    try:
        if not math.isfinite(args.seconds_budget) or args.seconds_budget <= 0 or args.memory_budget_mib <= 0:
            raise ValueError("Performance budgets must be positive.")
        check(args.cli.resolve(strict=True), args.desktop.resolve(strict=True) if args.desktop else None,
              args.report, args.seconds_budget, args.memory_budget_mib)
    except Unsupported as error:
        print(f"SKIP: {error}", file=sys.stderr)
        return 77
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"Offline check failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
