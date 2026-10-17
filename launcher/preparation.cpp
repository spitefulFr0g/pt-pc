#include "preparation.h"
#include "session.h"
#include "setup_core.h"
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <fstream>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace pt::launcher {
namespace {
namespace fs = std::filesystem;
std::mutex hooksMutex;
constexpr auto journalName = ".pt-preparation.json";
#ifdef _WIN32
constexpr auto platform = "windows";
constexpr auto program = "pt.exe";
constexpr auto helper = "PT.PkgExtract.exe";
#else
constexpr auto platform = "linux";
constexpr auto program = "pt";
constexpr auto helper = "PT.PkgExtract";
#endif
QString qpath(const fs::path& p) { return QString::fromStdU32String(p.u32string()); }
fs::path path(const QString& s) { return fs::path(s.toStdU32String()); }
[[noreturn]] void fail(const QString& s) { throw std::runtime_error(s.toStdString()); }
struct Cancelled {};
void check(const std::function<bool()>& cancel) { if (cancel && cancel()) throw Cancelled{}; }
// Inspect each component, including Windows junction/reparse points. No destination
// may escape through an existing link, even when the final file does not exist.
void safe(const fs::path& p) {
    fs::path current;
    for (const auto& component : fs::absolute(p).lexically_normal()) {
        current /= component;
        std::error_code ec;
        auto st = fs::symlink_status(current, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) fail("Cannot inspect " + qpath(current));
        if (fs::is_symlink(st)) fail("Linked preparation path is unsafe: " + qpath(current));
#ifdef _WIN32
        DWORD attr = GetFileAttributesW(current.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
            fail("Reparse point in preparation path: " + qpath(current));
#endif
        if (fs::exists(st) && !fs::is_directory(st) && current != p.lexically_normal())
            fail("Non-directory preparation path: " + qpath(current));
    }
}
bool present(const fs::path& p) { safe(p); return fs::exists(p); }
void ordinary(const fs::path& p) {
    safe(p);
    if (!fs::is_regular_file(p) || fs::hard_link_count(p) != 1) fail("Expected an ordinary, unlinked file: " + qpath(p));
}
QByteArray digest(const fs::path& p, const std::function<bool()>& cancel = {}) {
    ordinary(p); QFile f(qpath(p));
    if (!f.open(QIODevice::ReadOnly)) fail("Cannot read " + qpath(p));
    QCryptographicHash h(QCryptographicHash::Sha256);
    while (!f.atEnd()) { check(cancel); auto bytes = f.read(1024 * 1024); if (bytes.isEmpty() && f.error() != QFile::NoError) fail("Read failed: " + qpath(p)); h.addData(bytes); }
    return h.result().toHex();
}
void write(const fs::path& p, const QByteArray& bytes) {
    safe(p); fs::create_directories(p.parent_path()); QSaveFile f(qpath(p)); f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) fail("Cannot atomically write " + qpath(p));
}
QByteArray read(const fs::path& p, qint64 limit = 1024 * 1024) {
    ordinary(p); QFile f(qpath(p)); if (!f.open(QIODevice::ReadOnly) || f.size() > limit) fail("Cannot read bounded preparation metadata: " + qpath(p)); return f.readAll();
}
QStringList allowed() { return {"CUSA01127/chunk1.psarc", "CUSA01127/texture.qar", "CUSA01127/pathid_list_ps4.bin", "CUSA01127/source.txt", "CUSA01127/sce_sys/pic1.png", "install-notes.txt", QString(platform) + "/data/pt.ini", QString(platform) + "/data/.pt-launcher-adopted"}; }
void saveJournal(const fs::path& root, const QJsonObject& j) { write(root / journalName, QJsonDocument(j).toJson(QJsonDocument::Compact)); }
fs::path stagePath(const fs::path& root, const QJsonObject& j) {
    const QString name = j["stage"].toString();
    if (j["version"].toInt() != 1 || !QRegularExpression("^\\.pt-preparation-[0-9a-f]{32}$").match(name).hasMatch()) fail("Invalid preparation journal. Retain it and restore the bundle from a trusted backup.");
    auto stage = root / path(name); safe(stage);
    if (!fs::is_directory(stage) || read(stage / ".owner") != name.toUtf8()) fail("Preparation ownership marker missing or changed. Retain the stage for manual recovery.");
    return stage;
}
void safeTree(const fs::path& stage) {
    for (const auto& e : fs::recursive_directory_iterator(stage)) {
        safe(e.path()); if (!e.is_directory()) ordinary(e.path());
    }
}
void validateEntries(const QJsonObject& j) {
    QStringList seen;
    if (!j["files"].isArray() || j["files"].toArray().size() > allowed().size()) fail("Invalid preparation file journal.");
    for (auto v : j["files"].toArray()) {
        auto f = v.toObject(); auto n = f["name"].toString();
        if (!allowed().contains(n) || seen.contains(n) || !QRegularExpression("^[0-9a-f]{64}$").match(f["new"].toString()).hasMatch() ||
            (!f["old"].toString().isEmpty() && !QRegularExpression("^[0-9a-f]{64}$").match(f["old"].toString()).hasMatch())) fail("Unsafe preparation journal entry.");
        seen << n;
    }
}
void cleanup(const fs::path& root, const fs::path& stage) {
    safeTree(stage);
    // Keep ownership until all other stage contents have been removed.
    for (const auto& e : fs::directory_iterator(stage)) if (e.path().filename() != ".owner") fs::remove_all(e.path());
    fs::remove(stage / ".owner"); fs::remove(stage); fs::remove(root / journalName);
}
// Hashes distinguish a rename that happened just before a crash from one that did
// not. Validate the entire rollback first, so ambiguous data is never discarded.
void rollback(const fs::path& root, const fs::path& stage, const QJsonObject& j) {
    const auto files = j["files"].toArray();
    for (auto v : files) {
        auto f = v.toObject(); auto rel = path(f["name"].toString()); auto dest = root / rel, backup = stage / "old" / rel;
        auto old = f["old"].toString().toLatin1(), next = f["new"].toString().toLatin1();
        if (present(backup) && (old.isEmpty() || digest(backup) != old)) fail("Preparation backup changed; retain stage and journal for manual recovery.");
        if (present(dest)) { auto d = digest(dest); if (d != old && d != next) fail("Preparation target changed; retain stage and journal for manual recovery."); }
        if (!old.isEmpty() && !present(backup) && (!present(dest) || digest(dest) != old)) fail("Preparation backup is missing; manual recovery required.");
    }
    for (qsizetype i = files.size(); i-- > 0;) {
        auto f = files[i].toObject(); auto rel = path(f["name"].toString()); auto dest = root / rel, backup = stage / "old" / rel;
        if (present(backup)) { if (present(dest)) fs::remove(dest); fs::rename(backup, dest); }
        else if (f["old"].toString().isEmpty() && present(dest)) fs::remove(dest);
    }
}
void recover(const fs::path& root) {
    safe(root / journalName);
    if (!fs::exists(root / journalName)) {
        for (const auto& e : fs::directory_iterator(root)) if (qpath(e.path().filename()).startsWith(".pt-preparation-")) fail("An unjournaled preparation stage remains. Retain it and inspect it before retrying.");
        return;
    }
    QJsonParseError error; auto doc = QJsonDocument::fromJson(read(root / journalName), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) fail("Damaged preparation journal; retain it for manual recovery.");
    auto j = doc.object(); validateEntries(j);
    auto phase = j["phase"].toString();
    // Only terminal phases permit an absent stage. A missing committing stage
    // may contain the only rollback copies and must always block.
    QString stageName = j["stage"].toString();
    if (j["version"].toInt() != 1 || !QRegularExpression("^\\.pt-preparation-[0-9a-f]{32}$").match(stageName).hasMatch()) fail("Invalid preparation stage name.");
    auto stage = root / path(stageName); safe(stage);
    if (phase == "initializing") {
        // The journal is published before creating staging. A crash can leave
        // no directory, an empty directory, or only its ownership marker.
        if (!j["files"].toArray().isEmpty()) fail("Invalid preparation initialization journal.");
        if (!fs::exists(stage)) { fs::remove(root / journalName); return; }
        if (fs::is_directory(stage) && fs::is_empty(stage)) { fs::remove(stage); fs::remove(root / journalName); return; }
        safeTree(stage);
        for (const auto& entry : fs::directory_iterator(stage))
            if (entry.path().filename() != ".owner") fail("Unexpected initialization files; retain the stage for recovery.");
        // The initializing journal owns this directory before .owner is
        // written. A direct ownership write can be empty or partial at death.
        const auto marker = read(stage / ".owner", 128);
        if (!stageName.toUtf8().startsWith(marker)) fail("Initialization ownership marker changed; retain the stage.");
        cleanup(root, stage); return;
    }
    if (phase == "complete") {
        for (auto v : j["files"].toArray()) { auto f = v.toObject(); if (digest(root / path(f["name"].toString())) != f["new"].toString().toLatin1()) fail("Completed preparation has changed; retain the journal for manual recovery."); }
    } else if (phase == "rolledback") {
        for (auto v : j["files"].toArray()) { auto f = v.toObject(); auto target = root / path(f["name"].toString()); auto old = f["old"].toString().toLatin1(); if (old.isEmpty() ? present(target) : (!present(target) || digest(target) != old)) fail("Rolled-back preparation has changed; retain the journal for manual recovery."); }
    }
    if (!fs::exists(stage) && (phase == "complete" || phase == "rolledback")) { fs::remove(root / journalName); return; }
    if ((phase == "complete" || phase == "rolledback") && fs::is_directory(stage) && fs::is_empty(stage)) { fs::remove(stage); fs::remove(root / journalName); return; }
    stage = stagePath(root, j); safeTree(stage);
    if (present(stage / ".helper-pid")) {
        bool ok = false; qint64 pid = read(stage / ".helper-pid", 32).toLongLong(&ok);
        if (!ok || pid <= 0 || pid > 0xffffffffLL) fail("Invalid extractor ownership metadata; manual recovery required.");
#ifdef _WIN32
        HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
        if (h) { DWORD state = WaitForSingleObject(h, 0); CloseHandle(h); if (state != WAIT_OBJECT_0) fail("An extractor process may still own this stage. Wait for it to exit before recovery."); }
        else if (GetLastError() != ERROR_INVALID_PARAMETER) fail("Cannot establish that the previous extractor exited; retain staging and retry recovery.");
#elif defined(__linux__)
        if (pid > 0x7fffffffLL) fail("Invalid extractor PID; manual recovery required.");
        if (kill(pid_t(pid), 0) == 0 || errno != ESRCH) fail("An extractor process may still own this stage. Wait for it to exit before recovery.");
#endif
    }
    if (phase == "committing") { rollback(root, stage, j); j["phase"] = "rolledback"; saveJournal(root, j); }
    else if (phase == "complete") {
        for (auto v : j["files"].toArray()) { auto f = v.toObject(); if (digest(root / path(f["name"].toString())) != f["new"].toString().toLatin1()) fail("Completed preparation has changed; inspect journal before cleanup."); }
    } else if (phase != "staging" && phase != "ready" && phase != "rolledback") fail("Unknown preparation phase; manual recovery required.");
    // Staged work is safely rolled back to the original bundle; the source remains
    // available for retry. No source or unknown bundle file is removed.
    if (phase == "staging" || phase == "ready") { j["phase"] = "rolledback"; saveJournal(root, j); }
    cleanup(root, stage);
}
struct HookScope {
    setup::Hooks old = setup::hooks; setup::Progress prior = setup::progress;
    ~HookScope() { setup::hooks = std::move(old); setup::progress = prior; }
};
#ifdef _WIN32
// Assign the job atomically in CreateProcess, before any helper instruction runs.
// Assignment after start would leave a crash window with an unowned extractor.
struct HelperJob {
    HANDLE handle = nullptr;
    std::vector<unsigned char> storage;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes = nullptr;
    STARTUPINFOEXW startup{};
    HelperJob() {
        handle = CreateJobObjectW(nullptr, nullptr);
        if (!handle) fail("Cannot establish extractor ownership (CreateJobObject). Retry after checking Windows process permissions.");
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(handle, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { CloseHandle(handle); handle = nullptr; fail("Cannot establish kill-on-close extractor ownership."); }
        SIZE_T size = 0; InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        storage.resize(size); attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if (!InitializeProcThreadAttributeList(attributes, 1, 0, &size)) { attributes = nullptr; CloseHandle(handle); handle = nullptr; fail("Cannot initialize extractor process ownership attributes."); }
        if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &handle, sizeof(handle), nullptr, nullptr)) {
            DeleteProcThreadAttributeList(attributes); attributes = nullptr; CloseHandle(handle); handle = nullptr;
            fail("Windows cannot assign extractor ownership at process creation. Preparation is blocked; use a supported Windows 10/11 system.");
        }
    }
    ~HelperJob() { if (attributes) DeleteProcThreadAttributeList(attributes); if (handle) CloseHandle(handle); }
};
#endif
void extract(const fs::path& root, const fs::path& stage, const fs::path& source,
    const std::function<bool()>& cancel, const std::function<void(const PreparationProgress&)>& progress) {
    auto exe = root / platform / "extractor" / helper;
    if (!present(exe)) fail("The bundled package extractor is missing: " + qpath(exe) + ". Restore the matching portable preview package and retry; no .NET SDK is required.");
    ordinary(exe);
    auto output = stage / "new" / "CUSA01127";
    QProcess process; process.setProgram(qpath(exe)); process.setArguments({qpath(source), qpath(output)}); process.setWorkingDirectory(qpath(stage));
#ifdef _WIN32
    HelperJob job;
    process.setCreateProcessArgumentsModifier([&](QProcess::CreateProcessArguments* args) {
        job.startup.StartupInfo = *reinterpret_cast<STARTUPINFOW*>(args->startupInfo);
        job.startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        job.startup.lpAttributeList = job.attributes;
        args->startupInfo = reinterpret_cast<Q_STARTUPINFO*>(&job.startup);
        args->flags |= EXTENDED_STARTUPINFO_PRESENT;
    });
#elif defined(__linux__)
    const pid_t parent = getpid();
    process.setChildProcessModifier([&process, parent] {
        if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0) process.failChildProcessModifier("Cannot establish extractor parent-death ownership", errno);
        if (getppid() != parent) _exit(127);
    });
#else
    fail("This platform cannot establish crash-safe extractor ownership. Preparation is blocked.");
#endif
    auto env = QProcessEnvironment::systemEnvironment();
    for (const auto& key : env.keys()) if (key.startsWith("QT_") || key.startsWith("QML") || key.startsWith("LD_") || key.startsWith("DYLD_") || key.startsWith("DOTNET_") || key == "LIBPATH") env.remove(key);
    process.setProcessEnvironment(env); process.setProcessChannelMode(QProcess::MergedChannels);
    QByteArray tail, log; QElapsedTimer total, idle; total.start(); idle.start(); process.start();
    auto drain = [&] { auto bytes = process.readAll(); if (!bytes.isEmpty()) { idle.restart(); tail = (tail + bytes).right(16384); if (log.size() < 1024 * 1024) log += bytes.left(1024 * 1024 - log.size()); } };
    try {
        // Bound startup and retain PID as a conservative restart guard. OS ownership
        // is already installed before the helper can write, including this window.
        QElapsedTimer startup; startup.start();
        while (process.state() == QProcess::Starting && startup.elapsed() < 10000) { check(cancel); process.waitForStarted(100); }
        if (process.state() == QProcess::Starting) fail("Package extractor did not start within 10 seconds. Restore the matching helper and retry.");
        if (process.processId() > 0) write(stage / ".helper-pid", QByteArray::number(process.processId()));
        while (process.state() != QProcess::NotRunning) {
            check(cancel); process.waitForFinished(100); drain();
            if (total.elapsed() > 3600000 || idle.elapsed() > 600000) fail("Extraction timed out. Check free space and source health, then retry. See .pt-preparation.log.");
            if (progress) progress({"Extracting package…", 0, 0});
        }
        if (present(stage / ".helper-pid")) fs::remove(stage / ".helper-pid");
        drain(); write(root / ".pt-preparation.log", log); write(stage / "new" / "install-extraction.log", log);
        if (process.error() == QProcess::FailedToStart || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
            fail("Package helper failed. Ensure the matching self-contained extractor is installed; encrypted retail packages require a supported dump. See .pt-preparation.log.\n" + process.errorString() + "\n" + QString::fromUtf8(tail));
    } catch (...) {
        if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(5000); }
        if (process.state() != QProcess::NotRunning) fail("Extractor could not be reaped. Retain staging and retry recovery after it exits.");
        if (present(stage / ".helper-pid")) fs::remove(stage / ".helper-pid");
        drain(); write(root / ".pt-preparation.log", log); throw;
    }
}
void adopt(const fs::path& root, const fs::path& fresh) {
    const fs::path data = fs::path(platform) / "data", marker = data / ".pt-launcher-adopted", ini = data / "pt.ini";
    if (present(root / marker)) { ordinary(root / marker); return; }
    QByteArray bytes = present(root / ini) ? read(root / ini) : QByteArray{};
    QString text = QString::fromUtf8(bytes); QStringList lines = text.split('\n');
    bool network = false, found = false; int insertion = -1;
    for (int i = 0; i < lines.size(); ++i) {
        QString t = lines[i].trimmed();
        if (t.startsWith('[') && t.endsWith(']')) { if (network && insertion < 0) insertion = i; network = t.mid(1, t.size()-2).trimmed().compare("network", Qt::CaseInsensitive) == 0; }
        if (network && QRegularExpression("^check_updates\\s*=", QRegularExpression::CaseInsensitiveOption).match(t).hasMatch()) { lines[i] = "check_updates=0"; found = true; }
        if (network && i == lines.size()-1 && insertion < 0) insertion = lines.size();
    }
    if (!found) { if (insertion >= 0) lines.insert(insertion, "check_updates=0"); else lines << "[network]" << "check_updates=0"; }
    write(fresh / ini, lines.join('\n').toUtf8()); write(fresh / marker, "upstream-update-default-adopted-v1\n");
}
void commit(const fs::path& root, const fs::path& stage, QJsonObject& j, const std::function<bool()>& cancel) {
    QJsonArray files;
    for (const auto& name : allowed()) {
        auto rel = path(name), src = stage / "new" / rel, dest = root / rel;
        if (!present(src)) continue;
        auto next = digest(src, cancel); QByteArray old;
        if (present(dest)) old = digest(dest, cancel);
        files.append(QJsonObject{{"name", name}, {"new", QString::fromLatin1(next)}, {"old", QString::fromLatin1(old)}});
    }
    j["files"] = files; j["phase"] = "ready"; saveJournal(root, j); check(cancel);
    j["phase"] = "committing"; saveJournal(root, j);
    // Cancellation is deferred through this bounded sequence of local renames.
    try {
        for (auto v : files) {
            auto f = v.toObject(); auto rel = path(f["name"].toString()), dest = root / rel, backup = stage / "old" / rel;
            safe(dest); fs::create_directories(dest.parent_path());
            if (!f["old"].toString().isEmpty()) { fs::create_directories(backup.parent_path()); fs::rename(dest, backup); }
            fs::rename(stage / "new" / rel, dest);
        }
        j["phase"] = "complete"; saveJournal(root, j);
    } catch (...) { rollback(root, stage, j); j["phase"] = "rolledback"; saveJournal(root, j); throw; }
    cleanup(root, stage);
}
}

bool PreparationNeedsRecovery(const fs::path& bundle) {
    try {
        safe(bundle / journalName); if (fs::exists(bundle / journalName)) return true;
        for (const auto& e : fs::directory_iterator(bundle)) if (qpath(e.path().filename()).startsWith(".pt-preparation-")) return true;
        return false;
    } catch (...) { return true; }
}
PreparationResult RecoverPreparation(const fs::path& bundle) {
    PreparationResult result;
    try {
        auto root = fs::absolute(bundle).lexically_normal(); safe(root);
        WithStoppedBundle(root, [&] { std::lock_guard guard(hooksMutex); recover(root); result.ready = setup::ArchivesUsable(root / "CUSA01127"); }, &result.error);
    } catch (const std::exception& e) { result.error = QString::fromUtf8(e.what()); }
    catch (...) { result.error = "Unexpected preparation recovery failure. Retain the journal and stage."; }
    return result;
}
PreparationResult PrepareGame(const fs::path& bundle, const fs::path& input, const std::vector<SupportedRuntime>& supported,
    const std::function<bool()>& cancelled, const std::function<void(const PreparationProgress&)>& progress) {
    PreparationResult result;
    try {
        auto root = fs::absolute(bundle).lexically_normal(); safe(root);
        WithStoppedBundle(root, [&] {
            std::lock_guard guard(hooksMutex); HookScope restore;
            setup::hooks.cancelled = [&] { check(cancelled); return false; };
            setup::hooks.report = [&](const std::string& s) { if (progress) progress({QString::fromStdString(s), 0, 0}); };
            setup::hooks.progress = [&](uint64_t d, uint64_t t) { if (progress) progress({"Copying game archives…", d, t}); };
            recover(root); check(cancelled);
            auto exe = root / platform / program; auto hash = digest(exe, cancelled).toStdString();
            bool verified = false; for (const auto& s : supported) if (s.platform == platform && s.sha256 == hash) verified = true;
            if (!verified) fail("The installed platform runtime is not a verified supported build. Install the matching runtime before preparing game data.");
            safe(root / "CUSA01127");
            bool adopted = setup::ArchivesUsable(root / "CUSA01127");
            if (adopted) { ordinary(root / "CUSA01127" / "chunk1.psarc"); ordinary(root / "CUSA01127" / "texture.qar"); }
            const auto requiredSpace = adopted ? 1024ull * 1024 : 3ull * 1024 * 1024 * 1024;
            if (fs::space(root).available < requiredSpace) fail(adopted ? "Not enough free space to adopt this bundle's settings." : "Preparation needs at least 3 GiB of free space in the portable bundle. Free space and retry.");
            QString name = ".pt-preparation-" + QUuid::createUuid().toString(QUuid::Id128);
            auto stage = root / path(name);
            QJsonObject j{{"version", 1}, {"stage", name}, {"phase", "initializing"}, {"files", QJsonArray{}}};
            saveJournal(root, j);
            try {
                if (!fs::create_directory(stage)) fail("Could not create private preparation staging.");
                QFile owner(qpath(stage / ".owner"));
                if (!owner.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
                    owner.write(name.toUtf8()) != name.toUtf8().size() || !owner.flush())
                    fail("Could not record preparation staging ownership.");
                owner.close();
                j["phase"] = "staging"; saveJournal(root, j);
                auto fresh = stage / "new"; fs::create_directory(fresh);
                if (!adopted) {
                    auto source = setup::ResolveSource(fs::absolute(input).lexically_normal()); check(cancelled);
                    if (source.kind == setup::SourceKind::Package) extract(root, stage, fs::absolute(source.path), cancelled, progress);
                    else {
                        ordinary(source.files.psarc); ordinary(source.files.qar); if (!source.files.pathid.empty()) ordinary(source.files.pathid);
                        setup::progress.Begin(fs::file_size(source.files.psarc) + fs::file_size(source.files.qar) + (source.files.pathid.empty() ? 0 : fs::file_size(source.files.pathid)));
                        setup::CopyArchives(source.files, fresh / "CUSA01127");
                    }
                    safeTree(stage);
                    auto assets = fresh / "CUSA01127";
                    ordinary(assets / "chunk1.psarc"); ordinary(assets / "texture.qar");
                    if (!setup::ArchivesUsable(assets)) fail("Extraction did not produce the recognized archive pair. Check the source and .pt-preparation.log.");
                    if (present(assets / "pathid_list_ps4.bin") && !setup::IsPathList(assets / "pathid_list_ps4.bin")) fail("Extracted path list is invalid.");
                    for (const auto& note : setup::WriteNotes(source, fresh)) result.notes << QString::fromStdString(note);
                    if (auto png = setup::FindSystemPng(source.path, "pic1.png")) write(assets / "sce_sys" / "pic1.png", QByteArray(png->data(), qsizetype(png->size())));
                } else result.notes << "Adopted the existing recognized archive pair without copying or extraction.";
                check(cancelled); adopt(root, fresh); commit(root, stage, j, cancelled); result.ready = true;
            } catch (...) {
                auto original = std::current_exception();
                // Recovery rolls back any interrupted commit before cleaning owned staging.
                recover(root); std::rethrow_exception(original);
            }
        }, &result.error);
    } catch (const Cancelled&) { result.cancelled = true; }
    catch (const std::exception& e) { result.error = QString::fromUtf8(e.what()); }
    catch (...) { result.error = "Unexpected preparation failure. Retry recovery before Play."; }
    return result;
}
}
