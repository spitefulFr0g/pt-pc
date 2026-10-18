"""Build a native launcher paired with the pinned upstream portable runtime."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

from fetch_launcher_preview_runtime import RELEASE_SOURCE_REVISION
from launcher_release import identity

REPO = Path(__file__).resolve().parents[2]
ZLIB_REVISION = "da607da739fa6047df13e66a2af6b8bec7c2a498"  # zlib 1.3.2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("windows", "linux"), required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--work", type=Path, default=REPO / "build/launcher-preview")
    parser.add_argument("--out", type=Path, default=REPO / "dist/launcher-preview")
    parser.add_argument("--runtime-archive", type=Path, help="Reuse a downloaded archive with the pinned checksum")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if platform.system() != {"windows": "Windows", "linux": "Linux"}[args.platform]:
        parser.error("Build each launcher natively on its corresponding runner")
    if platform.machine().lower() not in ("amd64", "x86_64") or args.jobs < 1:
        parser.error("Use an x86_64 host and a positive --jobs count")
    identity(args.version)
    work = args.work.resolve()
    evidence = work / "evidence"
    evidence.mkdir(parents=True, exist_ok=True)
    launcher, stage, runtime = work / "launcher", work / "launcher-stage", work / "runtime-stage"
    env = os.environ.copy()
    cache_args = []
    if args.platform == "linux" and env.get("CCACHE_DIR"):
        ccache = shutil.which("ccache")
        if not ccache:
            raise RuntimeError("CCACHE_DIR is configured but ccache is unavailable")
        env.update(CCACHE_COMPILERCHECK="content", CCACHE_BASEDIR=str(REPO),
                   CCACHE_MAXSIZE="2G")
        cache_args = [f"-DCMAKE_{language}_COMPILER_LAUNCHER={ccache}"
                      for language in ("C", "CXX")]
    timings = []
    with (evidence / "build.log").open("w", encoding="utf-8") as log:
        def run(*command, phase="setup"):
            command = [str(part) for part in command]
            print("+", command, flush=True)
            log.write(repr(command) + "\n")
            started = time.monotonic()
            status = "failed"
            try:
                with subprocess.Popen(command, cwd=REPO, env=env, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, text=True, errors="replace") as process:
                    for line in process.stdout:
                        print(line, end="", flush=True)
                        log.write(line)
                    if process.wait():
                        raise subprocess.CalledProcessError(process.returncode, command)
                status = "ok"
            finally:
                record = {"phase": phase, "command": command, "status": status,
                          "seconds": round(time.monotonic() - started, 3)}
                timings.append(record)
                (evidence / "timings.json").write_text(json.dumps(timings, indent=2) + "\n")
                message = "TIMING " + json.dumps(record)
                print(message, flush=True)
                log.write(message + "\n")
                log.flush()

        revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
        # Linux rebuilds the pinned runtime's game sources with different
        # build configuration to enable OpenXR. Verify game code stays at the
        # source pin while allowing the scoped platform build changes.
        if subprocess.run(["git", "cat-file", "-e", RELEASE_SOURCE_REVISION + "^{commit}"],
                          cwd=REPO, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
            run("git", "fetch", "--no-tags", "--depth=1", "https://github.com/LoreanXavier/pt-pc.git",
                RELEASE_SOURCE_REVISION)
        run("git", "diff", "--exit-code", RELEASE_SOURCE_REVISION, revision, "--", "src")
        (evidence / "environment.json").write_text(json.dumps({
            "launcher_revision": revision, "platform": platform.platform(), "python": sys.version,
            "qt_prefix": env.get("QT_ROOT"), "runtime_release_source_revision": RELEASE_SOURCE_REVISION,
            "cmake": subprocess.check_output(["cmake", "--version"], text=True).strip(),
        }, indent=2) + "\n", encoding="utf-8")
        shutil.rmtree(runtime, ignore_errors=True)
        fetch = [sys.executable, REPO / "tools/ci/fetch_launcher_preview_runtime.py",
                 "--platform", args.platform, "--out", runtime]
        if args.runtime_archive:
            fetch += ["--archive", args.runtime_archive.resolve()]
        run(*fetch)
        if args.platform == "linux":
            # The release archive supplies pinned auxiliary assets, but its game
            # was built without VR. Build unchanged engine source with OpenXR.
            game = work / "game"
            run("cmake", "-S", REPO, "-B", game, "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=gcc-13",
                "-DCMAKE_CXX_COMPILER=g++-13", "-DPT_OPENXR=ON", "-DPT_UPSCALERS=OFF",
                *cache_args, phase="runtime-configure")
            run("cmake", "--build", game, "--target", "pt", "pt_lisa_date_assets",
                "--parallel", args.jobs, phase="runtime-build")
            for name in ("pt", "libopenxr_loader.so.1"):
                shutil.copy2(game / name, runtime / name)
            for name in ("shaders", "fonts", "voice", "lisa_date", "licenses"):
                shutil.copytree(game / name, runtime / name, dirs_exist_ok=True)
            sdk = game / "_deps/upscaler-sdks/openxr-1.1.63"
            for source, name in ((sdk / "LICENSE", "OpenXR-LICENSE.txt"),
                                 (sdk / "LICENSES/MIT.txt", "OpenXR-MIT.txt")):
                shutil.copy2(source, runtime / "licenses" / name)
            origin_path = runtime / "runtime-origin.json"
            origin = json.loads(origin_path.read_text())
            origin["asset_archive_executable_sha256"] = origin["executable_sha256"]
            origin.update({"kind": "source-build-with-pinned-release-assets",
                           "source_revision": revision,
                           "build_options": {"PT_OPENXR": "ON", "PT_UPSCALERS": "OFF"},
                           "compiler": subprocess.check_output(["g++-13", "--version"], text=True).strip(),
                           "executable_sha256": hashlib.sha256((runtime / "pt").read_bytes()).hexdigest(),
                           "openxr_loader_sha256": hashlib.sha256((runtime / "libopenxr_loader.so.1").read_bytes()).hexdigest()})
            origin_path.write_text(json.dumps(origin, indent=2) + "\n")
        executable = runtime / ("pt.exe" if args.platform == "windows" else "pt")
        with executable.open("rb") as binary:
            runtime_hash = hashlib.file_digest(binary, "sha256").hexdigest()

        configure = ["cmake", "-S", REPO / "launcher", "-B", launcher, "-G", "Ninja",
                     "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=" + str(stage),
                     f"-DPT_SUPPORTED_{args.platform.upper()}_SHA256={runtime_hash}",
                     f"-DPT_LAUNCHER_VERSION={args.version}"]
        if args.platform == "windows":
            # Build Qt's zlib dependency with the same dynamic MSVC C runtime.
            source, zlib_build, zlib_install = work / "zlib-source", work / "launcher-zlib", work / "zlib-install"
            run("git", "init", source)
            run("git", "-C", source, "fetch", "--depth=1", "https://github.com/madler/zlib.git", ZLIB_REVISION)
            run("git", "-C", source, "checkout", "--detach", ZLIB_REVISION)
            run("cmake", "-S", source, "-B", zlib_build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_C_COMPILER=cl", "-DCMAKE_INSTALL_PREFIX=" + str(zlib_install),
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL", "-DZLIB_BUILD_SHARED=OFF",
                "-DZLIB_BUILD_STATIC=ON", "-DZLIB_BUILD_TESTING=OFF", "-DZLIB_BUILD_EXAMPLES=OFF",
                "-DZLIB_INSTALL=ON")
            run("cmake", "--build", zlib_build, "--parallel", args.jobs)
            run("cmake", "--install", zlib_build)
            # zlib 1.3.2 installs zs.lib; CMake 3.31's FindZLIB does not search
            # that name. Select the library we just built explicitly.
            zlib_library = zlib_install / "lib/zs.lib"
            if not zlib_library.is_file():
                raise FileNotFoundError(zlib_library)
            configure += ["-DCMAKE_CXX_COMPILER=cl", "-DZLIB_ROOT=" + str(zlib_install),
                          "-DZLIB_USE_STATIC_LIBS=ON", "-DZLIB_LIBRARY_RELEASE=" + str(zlib_library)]
        else:
            configure += ["-DCMAKE_CXX_COMPILER=g++-13", "-DPT_SUPPORTED_LINUX_VR=ON"]
        configure += cache_args
        run(*configure, phase="launcher-configure")
        run("cmake", "--build", launcher, "--parallel", args.jobs, phase="launcher-build")
        env["QT_QPA_PLATFORM"] = "offscreen"
        run("ctest", "--test-dir", launcher, "--output-on-failure", "--timeout", "90", "-R",
            "^launcher_(inspection|readiness|session|preparation|preparation_ui|preparation_process|settings|settings_dialog)$", phase="native-tests")
        shutil.rmtree(stage, ignore_errors=True)
        run("cmake", "--install", launcher, phase="launcher-install")
        run(sys.executable, REPO / "tools/ci/package_launcher_preview.py",
            "--platform", args.platform, "--version", args.version, "--revision", revision,
            "--runtime-stage", runtime, "--launcher-stage", stage, "--out", args.out.resolve(), phase="packaging")


if __name__ == "__main__":
    main()
