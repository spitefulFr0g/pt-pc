"""Build the real game first, then a launcher pinned to that exact executable."""

import argparse
import hashlib
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("windows", "linux"), required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--work", type=Path, default=REPO / "build/launcher-preview")
    parser.add_argument("--out", type=Path, default=REPO / "dist/launcher-preview")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if (args.platform == "windows") != (os.name == "nt"):
        parser.error("Build each platform natively on its corresponding runner")
    if platform.machine().lower() not in ("amd64", "x86_64"):
        parser.error("Preview bundles require an x86_64 build host")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    work = args.work.resolve()
    evidence = work / "evidence"
    evidence.mkdir(parents=True, exist_ok=True)
    runtime = work / "runtime"
    launcher = work / "launcher"
    stage = work / "launcher-stage"
    env = os.environ.copy()
    # The upstream Linux FSR shader compiler runs through Wine at build time only.
    env.setdefault("WINEDEBUG", "-all")
    env.setdefault("WINEPREFIX", str(work / "wine"))

    with (evidence / "build.log").open("w", encoding="utf-8") as log:
        def run(*command):
            command = [str(part) for part in command]
            print("+", command, flush=True)
            log.write(repr(command) + "\n")
            with subprocess.Popen(command, cwd=REPO, env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, errors="replace") as process:
                for line in process.stdout:
                    print(line, end="", flush=True)
                    log.write(line)
                if process.wait():
                    raise subprocess.CalledProcessError(process.returncode, command)

        revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
        configure = ["cmake", "-S", REPO, "-B", runtime, "-G", "Ninja",
                     "-DCMAKE_BUILD_TYPE=Release", "-DPT_UPSCALERS=ON", "-DPT_GAMEPLUS=ON"]
        if args.platform == "windows":
            configure += ["-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl",
                          "-DPT_REQUIRE_UPSCALERS=ON"]
        run(*configure)
        # pt's post-build operations stage the fonts, shaders and runtime libraries.
        run("cmake", "--build", runtime, "--parallel", args.jobs, "--target", "pt", "pt_release")
        executable = runtime / ("pt_release.exe" if args.platform == "windows" else "pt_release")
        with executable.open("rb") as binary:
            runtime_hash = hashlib.file_digest(binary, "sha256").hexdigest()

        launcher_config = ["cmake", "-S", REPO / "launcher", "-B", launcher, "-G", "Ninja",
                           "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=" + str(stage),
                           f"-DPT_SUPPORTED_{args.platform.upper()}_SHA256={runtime_hash}"]
        if args.platform == "windows":
            # Qt's MSVC SDK uses the dynamic C runtime; build a matching private zlib.
            zlib_build = work / "launcher-zlib"
            zlib_install = work / "zlib-install"
            run("cmake", "-S", runtime / "_deps/zlib-src", "-B", zlib_build, "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=" + str(zlib_install),
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL", "-DZLIB_BUILD_SHARED=OFF",
                "-DZLIB_BUILD_STATIC=ON", "-DZLIB_BUILD_TESTING=OFF", "-DZLIB_BUILD_EXAMPLES=OFF",
                "-DZLIB_INSTALL=ON")
            run("cmake", "--build", zlib_build, "--parallel", args.jobs)
            run("cmake", "--install", zlib_build)
            launcher_config += ["-DCMAKE_C_COMPILER=cl", "-DCMAKE_CXX_COMPILER=cl",
                                "-DZLIB_ROOT=" + str(zlib_install), "-DZLIB_USE_STATIC_LIBS=ON"]
        run(*launcher_config)
        run("cmake", "--build", launcher, "--parallel", args.jobs)
        # Existing inspection tests exercise the public operation through QApplication.
        env["QT_QPA_PLATFORM"] = "offscreen"
        test_pattern = "^launcher_inspection$" if args.platform == "windows" else "launcher_"
        run("ctest", "--test-dir", launcher, "--output-on-failure", "-R", test_pattern)
        shutil.rmtree(stage, ignore_errors=True)
        run("cmake", "--install", launcher)
        run(sys.executable, REPO / "tools/ci/package_launcher_preview.py",
            "--platform", args.platform, "--version", args.version, "--revision", revision,
            "--runtime-build", runtime, "--launcher-stage", stage, "--out", args.out.resolve())


if __name__ == "__main__":
    main()
