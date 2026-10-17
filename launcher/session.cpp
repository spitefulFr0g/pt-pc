#include "session.h"
#include <QFile>
#include <QLockFile>
#include <QProcess>
#include <fstream>
#include <sstream>
#include <filesystem>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#endif
namespace pt::launcher {
namespace fs = std::filesystem;
static QString qpath(const fs::path& p) { return QString::fromStdU32String(p.u32string()); }
static fs::path bundleAbsolute(const fs::path& p) {
    std::error_code error;
    const auto canonical = fs::weakly_canonical(p, error);
    return error ? fs::absolute(p).lexically_normal() : canonical;
}
static fs::path program(const fs::path& root) {
#ifdef _WIN32
    return root / "windows" / "pt.exe";
#else
    return root / "linux" / "pt";
#endif
}
static QString logPath(const fs::path& root) {
#ifdef _WIN32
    return qpath(root / "windows" / "data" / "pt.log");
#else
    return qpath(root / "linux" / "data" / "pt.log");
#endif
}
SessionState InspectSession(const fs::path& bundle) {
    const fs::path root = bundleAbsolute(bundle), target = program(root);
    SessionState result; result.bundle = root; result.log = logPath(root);
#ifdef __linux__
    std::error_code ec;
    if (!fs::is_directory("/proc", ec) || ec) { result.error = "Cannot inspect Linux processes."; return result; }
    result.activity = Activity::Stopped;
    for (fs::directory_iterator it("/proc", fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) continue;
        std::ifstream cmd(it->path() / "cmdline", std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(cmd)), {});
        const auto exe = fs::read_symlink(it->path() / "exe", ec);
        const bool exeKnown = !ec;
        const std::string exeName = exeKnown ? exe.filename().string() : "";
        const bool scriptInterpreter = exeName == "sh" || exeName == "dash" || exeName == "bash" || exeName == "zsh" || exeName == "ksh";
        // A process can exec through a script interpreter; its command line still names the script.
        const std::string candidate = target.string();
        bool matches = exeKnown && (exe == target || exe.string() == candidate + " (deleted)");
        int argumentIndex = 0;
        for (size_t pos = 0; scriptInterpreter && pos < bytes.size(); ++argumentIndex) {
            const size_t end = bytes.find('\0', pos);
            const std::string arg = bytes.substr(pos, end == std::string::npos ? end : end - pos);
            if (argumentIndex <= 2 && arg == candidate) matches = true;
            else if (argumentIndex <= 2 && !arg.empty() && arg[0] != '-' && fs::path(arg).filename() == target.filename()) {
                std::error_code path_error;
                const auto cwd = fs::read_symlink(it->path() / "cwd", path_error);
                if (!path_error && fs::weakly_canonical(cwd / arg, path_error) == fs::weakly_canonical(target, path_error) && !path_error) matches = true;
            }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
        if (bytes.empty() && ec) {
            std::ifstream comm(it->path() / "comm"); std::string processName; std::getline(comm, processName);
            if (processName == target.filename().string()) {
                result.activity = Activity::Uncertain; result.error = "A game process cannot be inspected."; return result;
            }
        }
        ec.clear();
        if (!matches) continue;
        std::ifstream stat(it->path() / "stat"); std::string line; std::getline(stat, line);
        const size_t close = line.rfind(')');
        if (close == std::string::npos || close + 4 >= line.size()) { result.activity = Activity::Uncertain; result.error = "Cannot establish game process identity."; return result; }
        std::istringstream fields(line.substr(close + 2)); std::string token, start;
        char state = '\0';
        for (int field = 3; field <= 22; ++field) { if (!(fields >> token)) { result.activity = Activity::Uncertain; result.error = "Cannot establish game process identity."; return result; } if (field == 3) state = token[0]; if (field == 22) start = token; }
        if (state == 'Z' || state == 'X') continue;
        if (result.activity == Activity::Running) { result.activity = Activity::Uncertain; result.error = "Multiple game processes found for this bundle."; return result; }
        result.activity = Activity::Running; result.pid = std::stoll(name); result.identity = QString::fromStdString(name + ":" + start);
    }
    if (ec) { result.activity = Activity::Uncertain; result.error = "Process enumeration failed."; }
#elif defined(_WIN32)
    result.activity = Activity::Stopped;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) { result.activity = Activity::Uncertain; result.error = "Cannot enumerate Windows processes."; return result; }
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot, &entry)) { CloseHandle(snapshot); result.activity = Activity::Uncertain; result.error = "Cannot enumerate Windows processes."; return result; }
    do {
        if (_wcsicmp(entry.szExeFile, L"pt.exe") != 0) continue;
        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, entry.th32ProcessID);
        if (!handle) { result.activity = Activity::Uncertain; result.error = "Cannot inspect a game process."; break; }
        wchar_t path[32768]; DWORD length = 32768;
        FILETIME created{}, exited{}, kernel{}, user{};
        bool known = QueryFullProcessImageNameW(handle, 0, path, &length) && GetProcessTimes(handle, &created, &exited, &kernel, &user);
        if (!known) { result.activity = Activity::Uncertain; result.error = "Cannot establish game process identity."; CloseHandle(handle); break; }
        if (WaitForSingleObject(handle, 0) == WAIT_TIMEOUT &&
            _wcsicmp(fs::path(path).lexically_normal().c_str(), target.c_str()) == 0) {
            if (result.activity == Activity::Running) { result.activity = Activity::Uncertain; result.error = "Multiple game processes found for this bundle."; CloseHandle(handle); break; }
            result.activity = Activity::Running; result.pid = entry.th32ProcessID;
            const unsigned long long ticks = (static_cast<unsigned long long>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
            result.identity = QString::number(result.pid) + ":" + QString::number(ticks);
        }
        CloseHandle(handle);
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
#else
    result.error = "Process identity inspection is unavailable on this platform.";
#endif
    return result;
}
static bool guarded(const fs::path& bundle, const std::function<void()>& action, QString* error) {
    const fs::path root = bundleAbsolute(bundle);
    if (!fs::is_directory(root)) { if (error) *error = "Bundle folder missing."; return false; }
    QLockFile lock(qpath(root / ".pt-launcher.lock")); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) { if (error) *error = "Another launcher operation is active for this bundle."; return false; }
    auto state = InspectSession(root);
    if (state.activity != Activity::Stopped) { if (error) *error = state.error.isEmpty() ? "Game activity blocks this operation." : state.error; return false; }
    action(); return true;
}
bool WithStoppedBundle(const fs::path& bundle, const std::function<void()>& mutation, QString* error) { return guarded(bundle, mutation, error); }
static SessionState play(const fs::path& bundle, const std::vector<SupportedRuntime>& supported, LaunchMode mode, bool tryAnyway) {
    const fs::path root = bundleAbsolute(bundle);
    auto current = InspectSession(root);
    if (current.activity != Activity::Stopped) return current;
    QString failure;
    bool started = guarded(root, [&] {
        // Another launcher may have crashed after this GUI last refreshed.
        // Inspect persisted mutation state while holding the same bundle lock
        // used by preparation, before allowing a partially committed game to run.
        std::error_code pendingError;
        if (fs::exists(root / ".pt-preparation.json", pendingError) || pendingError) {
            failure = "Game preparation needs recovery. Reopen the bundle or import your source again before Play.";
            return;
        }
        for (fs::directory_iterator it(root, pendingError), end; !pendingError && it != end; it.increment(pendingError)) {
            if (qpath(it->path().filename()).startsWith(".pt-preparation-")) {
                failure = "An interrupted preparation remains. Reopen the bundle to recover before Play.";
                return;
            }
        }
        if (pendingError) { failure = "Cannot inspect preparation state. Check folder permissions before Play."; return; }
        const auto inspected = InspectBundle(root, supported);
#ifdef _WIN32
        const auto& runtime = inspected.windows_runtime;
#else
        const auto& runtime = inspected.linux_runtime;
#endif
        if (!inspected.archives || !runtime.present) { failure = "Game archives or runtime are unavailable."; return; }
        if (!runtime.verified && !tryAnyway) { failure = "The runtime build is unverified. Choose the explicit Try anyway action to proceed."; return; }
        if (mode == LaunchMode::VR) {
            const auto readiness = CheckReadiness(inspected, LaunchMode::VR);
            if (readiness.state == ReadinessState::Blocked ||
                (readiness.state == ReadinessState::Unknown && !tryAnyway)) {
                failure = readiness.explanation;
                return;
            }
        }
        const auto executable = program(root);
        const QString log = logPath(root);
        std::error_code directoryError;
        fs::create_directories(executable.parent_path() / "data", directoryError);
        if (directoryError) { failure = "Cannot create the runtime log folder."; return; }
        QFile fresh(log);
        if (!fresh.open(QIODevice::WriteOnly | QIODevice::Truncate)) { failure = "Cannot create a fresh game log: " + fresh.errorString(); return; }
        fresh.close();
        QStringList arguments{"--game", qpath(root / "CUSA01127"), "--save-dir", qpath(root / "saves"), "--mods", qpath(root / "mods"),
            "--settings", qpath(executable.parent_path() / "data" / "pt.ini"), "--log", log,
            mode == LaunchMode::VR ? "--vr" : "--no-vr", "--no-update-check"};
        qint64 pid = 0;
        if (!QProcess::startDetached(qpath(executable), arguments, qpath(executable.parent_path()), &pid)) failure = "Could not start the game process. Open the game log and retry.";
    }, &failure);
    auto result = InspectSession(root);
    if (started && failure.isEmpty() && result.activity == Activity::Stopped)
        failure = "Game process exited immediately. Open the game log and retry.";
    if (!started || !failure.isEmpty()) result.error = failure;
    return result;
}
SessionState PlayFlat(const fs::path& bundle, const std::vector<SupportedRuntime>& supported, bool tryAnyway) {
    return play(bundle, supported, LaunchMode::Flat, tryAnyway);
}
SessionState PlayVR(const fs::path& bundle, const std::vector<SupportedRuntime>& supported, bool tryAnyway) {
    return play(bundle, supported, LaunchMode::VR, tryAnyway);
}
bool ReturnToGame(const SessionState& session, QString* error) {
#ifdef __linux__
    auto fresh = InspectSession(session.bundle);
    if (fresh.activity == Activity::Running && fresh.identity == session.identity && fresh.pid > 0) {
        // X11 permits focus through the window manager. Wayland compositors normally reject this.
        if (qEnvironmentVariable("XDG_SESSION_TYPE") == "x11") {
            QProcess search;
            search.start("xdotool", {"search", "--pid", QString::number(fresh.pid)});
            if (search.waitForFinished(1500) && search.exitCode() == 0) {
                const auto windows = QString::fromLocal8Bit(search.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
                for (const auto& id : windows) {
                    QProcess activate;
                    activate.start("xdotool", {"windowactivate", "--sync", id});
                    if (activate.waitForFinished(1500) && activate.exitCode() == 0) return true;
                }
            }
        }
        if (error) *error = "Use your desktop or Gaming Mode window switcher to return to the game.";
        return false;
    }
#endif
#ifdef _WIN32
    auto fresh = InspectSession(session.bundle);
    if (fresh.activity == Activity::Running && fresh.identity == session.identity) {
        struct WindowMatch { DWORD pid; HWND window; } match{static_cast<DWORD>(fresh.pid), nullptr};
        EnumWindows([](HWND window, LPARAM data) -> BOOL {
            auto* match = reinterpret_cast<WindowMatch*>(data); DWORD pid = 0;
            GetWindowThreadProcessId(window, &pid);
            if (pid == match->pid && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) { match->window = window; return FALSE; }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&match));
        if (match.window) { ShowWindow(match.window, SW_RESTORE); return SetForegroundWindow(match.window) != 0; }
    }
#endif
    if (error) *error = "No game window can be resumed.";
    return false;
}
}
