#include "inspection.h"
#include "session.h"
#include "preparation.h"
#include "settings_dialog.h"
#include "readiness.h"
#include <QApplication>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QResizeEvent>
#include <QPainter>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMessageBox>
#include <QProgressBar>
#include <QPointer>
#include <QThread>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <optional>

class Artwork : public QWidget {
public:
    explicit Artwork(const QString& path) : picture(path) {}
    QSize sizeHint() const override { return {320, 500}; }
protected:
    void paintEvent(QPaintEvent*) override {
        if (picture.isNull()) return;
        QPainter painter(this);
        const double scale = std::max(double(width()) / picture.width(), double(height()) / picture.height());
        const double source_w = width() / scale, source_h = height() / scale;
        QRectF source((picture.width() - source_w) / 2, (picture.height() - source_h) / 2, source_w, source_h);
        painter.drawPixmap(rect(), picture, source);
    }
private:
    QPixmap picture;
};
class ResponsiveWindow : public QWidget {
public:
    QBoxLayout* layout = nullptr;
    QWidget* artwork = nullptr;
    std::function<bool()> canClose;
    std::function<void(const QString&)> importSource;
protected:
    void closeEvent(QCloseEvent* event) override {
        if (canClose && !canClose()) event->ignore();
        else QWidget::closeEvent(event);
    }
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1 &&
            event->mimeData()->urls().first().isLocalFile()) event->acceptProposedAction();
    }
    void dropEvent(QDropEvent* event) override {
        if (importSource && event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1 &&
            event->mimeData()->urls().first().isLocalFile()) {
            importSource(event->mimeData()->urls().first().toLocalFile());
            event->acceptProposedAction();
        }
    }
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        const bool narrow = width() < 700;
        if (layout) layout->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        if (artwork) { artwork->setMaximumHeight(narrow ? 180 : 16777215); artwork->setMinimumWidth(narrow ? 0 : 180); }
    }
};
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::vector<pt::launcher::SupportedRuntime> supported;
    if (QString(PT_SUPPORTED_LINUX_SHA256).size() == 64) supported.push_back({"linux", PT_SUPPORTED_LINUX_SHA256, "bundled", PT_SUPPORTED_LINUX_VR != 0});
    if (QString(PT_SUPPORTED_WINDOWS_SHA256).size() == 64) supported.push_back({"windows", PT_SUPPORTED_WINDOWS_SHA256, "bundled", PT_SUPPORTED_WINDOWS_VR != 0});
    std::filesystem::path chosen;
    std::optional<pt::launcher::BundleInspection> cachedInspection;
    QString lastFailure;
    bool previousRunning = false;
    QSettings preferences("pt-pc", "pt-launcher");
    const QString lastBundle = preferences.value("lastBundle").toString();
    if (!lastBundle.isEmpty()) chosen = std::filesystem::path(lastBundle.toStdU32String());
    auto applicationFolder = std::filesystem::path(QCoreApplication::applicationDirPath().toStdU32String());
#ifdef _WIN32
    const auto bundledRoot = applicationFolder;
    const auto runtimeRelative = std::filesystem::path("windows/pt.exe");
#else
    const auto bundledRoot = applicationFolder.filename() == "libexec" ? applicationFolder.parent_path() : applicationFolder;
    const auto runtimeRelative = std::filesystem::path("linux/pt");
#endif
    std::error_code pathError;
    // A newly extracted preview should prepare its own bundle. Remember an
    // explicitly opened bundle only for this particular portable launcher.
    const auto bundleOwner = QString::fromStdU32String(bundledRoot.u32string());
    if (std::filesystem::is_regular_file(bundledRoot / runtimeRelative, pathError) &&
        (preferences.value("lastBundleOwner").toString() != bundleOwner || chosen.empty() ||
         !std::filesystem::is_regular_file(chosen / runtimeRelative, pathError))) chosen = bundledRoot;
#ifdef PT_LAUNCHER_TEST_BUILD
    QString testMode;
    if (argc == 5 && QString::fromLocal8Bit(argv[1]).startsWith("--test-") && QString::fromLocal8Bit(argv[3]) == "--test-supported-sha256") {
        testMode = QString::fromLocal8Bit(argv[1]);
        chosen = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdU32String());
        if (testMode != "--test-flat-unknown-control") supported.push_back({"linux", argv[4], "fixture", true});
    }
#endif
    const QString assets = QCoreApplication::applicationDirPath() + "/assets/";
    QFontDatabase::addApplicationFont(assets + "PirataOne-Regular.ttf");
    ResponsiveWindow window; window.setWindowTitle("P.T. Launcher"); window.resize(1000, 680);
    window.setAcceptDrops(true);
    window.setStyleSheet("QWidget{background:#10271f;color:#e5e1d6;font:16px sans-serif} QPushButton{background:#294738;border:1px solid #688674;border-radius:7px;padding:11px;text-align:left} QPushButton:hover{background:#41634d} QPushButton:disabled{color:#9ba99d} QLabel#title{font:60px 'Pirata One'} QLabel#warning{color:#e8c495}");
    auto* layout = new QHBoxLayout(&window);
    window.layout = layout;
    auto* artwork = new Artwork(assets + "spooky-forest.jpg"); window.artwork = artwork;
    artwork->setMinimumWidth(180);
    artwork->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(artwork, 2);
    auto* panel = new QWidget; auto* controls = new QVBoxLayout(panel); controls->setSpacing(14); controls->setContentsMargins(28,30,28,30);
    auto* title = new QLabel("P.T."); title->setObjectName("title"); controls->addWidget(title);
    controls->addWidget(new QLabel("A quiet place to return.")); controls->addStretch();
    auto* warning = new QLabel("Choose your P.T. .pkg in Game files, or drop it here."); warning->setObjectName("warning"); warning->setWordWrap(true); controls->addWidget(warning);
    auto* play = new QPushButton("Play"); play->setObjectName("play"); play->setEnabled(false); controls->addWidget(play);
    auto* flatReadinessLabel = new QLabel; flatReadinessLabel->setWordWrap(true); controls->addWidget(flatReadinessLabel);
    auto* resume = new QPushButton("Return to game"); resume->setObjectName("returnToGame"); resume->hide(); controls->addWidget(resume);
    auto* retry = new QPushButton("Retry"); retry->hide(); controls->addWidget(retry);
    auto* logButton = new QPushButton("Open game log"); logButton->hide(); controls->addWidget(logButton);
    auto* vr = new QPushButton("Play in VR"); vr->setEnabled(false); controls->addWidget(vr);
    auto* vrReadiness = new QLabel; vrReadiness->setWordWrap(true); controls->addWidget(vrReadiness);
    auto* retryVr = new QPushButton("Retry VR readiness"); retryVr->hide(); controls->addWidget(retryVr);
    auto* settings = new QPushButton("Settings"); settings->setObjectName("settings"); settings->setEnabled(false); controls->addWidget(settings);
    auto* files = new QPushButton("Game files — choose .pkg"); controls->addWidget(files);
    auto* folderFiles = new QPushButton("Import extracted game folder"); controls->addWidget(folderFiles);
    auto* openBundle = new QPushButton("Open existing portable bundle"); controls->addWidget(openBundle);
    auto* preparationBar = new QProgressBar; preparationBar->hide(); controls->addWidget(preparationBar);
    auto* cancelPreparation = new QPushButton("Cancel preparation"); cancelPreparation->hide(); controls->addWidget(cancelPreparation);
    auto* updates = new QPushButton("Updates"); updates->setEnabled(false); controls->addWidget(updates);
    auto* details = new QLabel; details->setWordWrap(true); controls->addWidget(details);
    bool preparing = false, closeAfterPreparation = false, preparationLog = false, recoveryBlocked = false;
    bool readinessPending = false, launchPending = false;
    pt::launcher::ModeReadiness flatReadiness, vrModeReadiness;
    QThread* readinessThread = nullptr;
    auto cancelRequested = std::make_shared<std::atomic<bool>>(false);
    QThread* preparationThread = nullptr;
    QPointer<QMessageBox> cancelPrompt;
#ifdef PT_LAUNCHER_TEST_BUILD
    QPointer<QWidget> preparationPromptFocus;
    bool preparationPromptProgressSeen = false;
#endif
    std::function<void()> refresh;
    auto readinessText = [](const QString& mode, const pt::launcher::ModeReadiness& state) {
        QString label = state.state == pt::launcher::ReadinessState::Ready ? "Ready" :
            state.state == pt::launcher::ReadinessState::Blocked ? "Blocked" : "Unknown";
        return mode + ": " + label + " — " + state.explanation;
    };
    refresh = [&] {
        if (chosen.empty() || preparing) return;
        auto session = pt::launcher::InspectSession(chosen);
        if (previousRunning && session.activity == pt::launcher::Activity::Stopped) {
            lastFailure = "Game process exited. Open the game log or retry Play.";
            retry->show(); logButton->show();
            cachedInspection.reset();
        }
        const bool pendingPreparation = pt::launcher::PreparationNeedsRecovery(chosen);
        if (recoveryBlocked && !pendingPreparation) { cachedInspection.reset(); retry->hide(); }
        recoveryBlocked = pendingPreparation;
        if (!cachedInspection) {
            play->setEnabled(false); vr->setEnabled(false);
            vrReadiness->setText("Checking VR prerequisites…");
            if (!readinessPending) {
                readinessPending = true;
                const auto bundle = chosen;
                readinessThread = QThread::create([&, bundle] {
                    auto inspection = pt::launcher::InspectBundle(bundle, supported);
                    auto flatState = pt::launcher::CheckReadiness(inspection, pt::launcher::LaunchMode::Flat);
                    auto vrState = pt::launcher::CheckReadiness(inspection, pt::launcher::LaunchMode::VR);
                    QMetaObject::invokeMethod(&window, [&, bundle, inspection = std::move(inspection), flatState, vrState] () mutable {
                        readinessPending = false; readinessThread = nullptr;
                        if (chosen == bundle) {
                            cachedInspection = std::move(inspection);
                            flatReadiness = flatState; vrModeReadiness = vrState;
                        }
                        refresh();
                    }, Qt::QueuedConnection);
                });
                QObject::connect(readinessThread, &QThread::finished, readinessThread, &QObject::deleteLater);
                readinessThread->start();
            }
            return;
        }
        const auto& inspection = *cachedInspection;
        previousRunning = session.activity == pt::launcher::Activity::Running;
        const bool runtime =
#ifdef _WIN32
            inspection.windows_runtime.verified;
#else
            inspection.linux_runtime.verified;
#endif
        play->setText(flatReadiness.state == pt::launcher::ReadinessState::Unknown ? "Try Play anyway" : "Play");
        play->setEnabled(!launchPending && !recoveryBlocked && session.activity == pt::launcher::Activity::Stopped && flatReadiness.state != pt::launcher::ReadinessState::Blocked);
        vr->setText(vrModeReadiness.state == pt::launcher::ReadinessState::Unknown ? "Try VR anyway" : "Play in VR");
        vr->setEnabled(!launchPending && !recoveryBlocked && session.activity == pt::launcher::Activity::Stopped && vrModeReadiness.state != pt::launcher::ReadinessState::Blocked);
        flatReadinessLabel->setText(readinessText("Flat", flatReadiness));
        vrReadiness->setText(readinessText("VR", vrModeReadiness));
        retryVr->setVisible(vrModeReadiness.state != pt::launcher::ReadinessState::Ready);
        retryVr->setEnabled(!launchPending && !recoveryBlocked && session.activity == pt::launcher::Activity::Stopped);
        settings->setEnabled(runtime && !preparing && !recoveryBlocked);
        settings->setToolTip(runtime ? "" : "Align with a matching supported runtime to edit settings.");
        resume->setVisible(session.activity == pt::launcher::Activity::Running);
        if (session.activity == pt::launcher::Activity::Running) warning->setText("Game process active. Return to the existing session.");
        else if (session.activity == pt::launcher::Activity::Uncertain) warning->setText(session.error.isEmpty() ? "Game activity uncertain. Play is blocked." : session.error);
        else if (recoveryBlocked) warning->setText("Game preparation is active or interrupted. Choose Recover preparation after it has stopped.");
        else if (!lastFailure.isEmpty()) warning->setText(lastFailure);
        else if (!inspection.archives) warning->setText("Add your P.T. .pkg in Game files, or drop it here. The launcher will prepare it for you.");
        else warning->setText(QString::fromStdString(inspection.error.empty() ?
            (runtime ? "" : inspection.linux_runtime.present || inspection.windows_runtime.present ?
              "Runtime unverified. Install a matching supported release to enable Play and Settings." :
              "Runtime missing. Add a matching platform runtime to this bundle.") : inspection.error));
        retry->setText(recoveryBlocked ? "Recover preparation" : "Retry");
        if (recoveryBlocked) retry->show();
        details->setText(QString("Archives: %1\nPath list: %2\nRetained source: %3\nSaves: %4\nMods: %5\nLinux: %6\nWindows: %7")
            .arg(inspection.archives ? "recognized" : "missing")
            .arg(inspection.path_list ? "recognized" : "missing")
            .arg(inspection.source ? "recognized" : "not found")
            .arg(inspection.saves ? "found" : "not found")
            .arg(inspection.mods ? "found" : "not found")
            .arg(inspection.linux_runtime.verified ? "verified" : inspection.linux_runtime.present ? "unverified" : "missing")
            .arg(inspection.windows_runtime.verified ? "verified" : inspection.windows_runtime.present ? "unverified" : "missing"));
    };
    auto confirmCancellation = [&](bool closing) {
        if (!preparing) return;
        if (cancelPrompt) { cancelPrompt->raise(); cancelPrompt->activateWindow(); return; }
        auto* prompt = new QMessageBox(QMessageBox::Question, "Cancel game preparation?",
            "Your original source and existing game files will be kept.", QMessageBox::NoButton, &window);
        auto* keep = prompt->addButton("Keep preparing", QMessageBox::RejectRole);
        auto* cancel = prompt->addButton(closing ? "Cancel and close" : "Cancel preparation", QMessageBox::DestructiveRole);
        prompt->setDefaultButton(keep); prompt->setEscapeButton(keep);
        prompt->setAttribute(Qt::WA_DeleteOnClose); cancelPrompt = prompt;
        QObject::connect(prompt, &QMessageBox::finished, &window, [&, prompt, cancel, closing](int) {
            if (preparing && prompt->clickedButton() == cancel) {
                cancelRequested->store(true); closeAfterPreparation = closing;
                cancelPreparation->setEnabled(false); warning->setText("Cancelling preparation…");
            }
        });
        prompt->open();
    };
    std::function<void(const QString&, bool)> prepare;
    prepare = [&](const QString& source, bool recoverOnly) {
        if (preparing) { warning->setText("Game preparation is already running."); return; }
        if (chosen.empty()) { warning->setText("Extract the portable preview first, then run its launcher."); return; }
        settings->setEnabled(false);
        preparing = true; cancelRequested->store(false); preparationLog = true;
        logButton->setText("Open preparation log");
        play->setEnabled(false); vr->setEnabled(false); retryVr->setEnabled(false); resume->hide(); retry->hide(); logButton->hide();
        files->setEnabled(false); folderFiles->setEnabled(false); openBundle->setEnabled(false);
        preparationBar->setRange(0, 0); preparationBar->show();
        cancelPreparation->setEnabled(!recoverOnly); cancelPreparation->show();
        warning->setText(recoverOnly ? "Recovering interrupted preparation…" : "Preparing your game…");
        details->clear(); lastFailure.clear();
        const auto bundle = chosen;
        const auto input = std::filesystem::path(source.toStdU32String());
        auto result = std::make_shared<pt::launcher::PreparationResult>();
        preparationThread = QThread::create([&, bundle, input, result, recoverOnly, cancelRequested] {
            if (recoverOnly) *result = pt::launcher::RecoverPreparation(bundle);
            else *result = pt::launcher::PrepareGame(bundle, input, supported,
                [cancelRequested] { return cancelRequested->load(); },
                [&](const pt::launcher::PreparationProgress& progress) {
                    QMetaObject::invokeMethod(&window, [&, progress, cancelRequested] {
                        if (!preparing || cancelRequested->load()) return;
                        warning->setText(progress.message);
#ifdef PT_LAUNCHER_TEST_BUILD
                        if (testMode == "--test-preparation-prompt" && cancelPrompt && progress.total && preparationPromptFocus) {
                            preparationPromptProgressSeen = true;
                            if (QApplication::focusWidget() != preparationPromptFocus) qFatal("Preparation progress changed Qt focus");
                        }
#endif
                        if (progress.total) {
                            preparationBar->setRange(0, 1000);
                            preparationBar->setValue(int(std::min(1.0, double(progress.done) / double(progress.total)) * 1000));
                        } else preparationBar->setRange(0, 0);
                    }, Qt::QueuedConnection);
                });
        });
        QObject::connect(preparationThread, &QThread::finished, &window, [&, result, bundle, recoverOnly] {
            preparing = false;
            if (cancelPrompt) cancelPrompt->reject();
            preparationBar->hide(); cancelPreparation->hide();
            files->setEnabled(true); folderFiles->setEnabled(true); openBundle->setEnabled(true);
            cachedInspection.reset(); previousRunning = false;
            recoveryBlocked = pt::launcher::PreparationNeedsRecovery(bundle);
            if (!result->error.isEmpty()) lastFailure = result->error;
            else if (result->cancelled) lastFailure = "Preparation cancelled. Your source and existing game files were kept.";
            else if (result->ready && !recoverOnly) lastFailure = "Your game is ready. Choose Play to start.";
            if (!result->notes.isEmpty()) lastFailure += "\n" + result->notes.join('\n');
            std::error_code logError;
            if (!result->error.isEmpty() && std::filesystem::is_regular_file(bundle / ".pt-preparation.log", logError)) logButton->show();
            preferences.setValue("lastBundle", QString::fromStdU32String(bundle.u32string()));
            preferences.setValue("lastBundleOwner", bundleOwner);
            preparationThread = nullptr;
            refresh();
            if (closeAfterPreparation) window.close();
        });
        QObject::connect(preparationThread, &QThread::finished, preparationThread, &QObject::deleteLater);
        preparationThread->start();
    };
    window.canClose = [&] {
        if (!preparing) return true;
        confirmCancellation(true); return false;
    };
    window.importSource = [&](const QString& source) { prepare(source, false); };
    QObject::connect(files, &QPushButton::clicked, &window, [&] {
        const QString source = QFileDialog::getOpenFileName(&window, "Choose your P.T. package", {}, "PS4 package (*.pkg);;All files (*)");
        if (!source.isEmpty()) prepare(source, false);
    });
    QObject::connect(folderFiles, &QPushButton::clicked, &window, [&] {
        const QString source = QFileDialog::getExistingDirectory(&window, "Choose your extracted P.T. game folder");
        if (!source.isEmpty()) prepare(source, false);
    });
    QObject::connect(openBundle, &QPushButton::clicked, &window, [&] {
        QString folder = QFileDialog::getExistingDirectory(&window, "Choose portable bundle");
        if (folder.isEmpty()) return;
        chosen = std::filesystem::path(folder.toStdU32String()); cachedInspection.reset(); lastFailure.clear(); previousRunning = false;
        recoveryBlocked = pt::launcher::PreparationNeedsRecovery(chosen);
        if (recoveryBlocked) prepare({}, true); else refresh();
        preferences.setValue("lastBundle", folder);
        preferences.setValue("lastBundleOwner", bundleOwner);
    });
    QObject::connect(cancelPreparation, &QPushButton::clicked, &window, [&] { confirmCancellation(false); });
    QObject::connect(play, &QPushButton::clicked, &window, [&] {
        lastFailure.clear(); preparationLog = false; logButton->setText("Open game log"); retry->hide(); logButton->hide();
        auto result = pt::launcher::PlayFlat(chosen, supported, flatReadiness.state == pt::launcher::ReadinessState::Unknown);
        refresh();
        if (!result.error.isEmpty()) { lastFailure = result.error; warning->setText(result.error); retry->show(); logButton->show(); }
    });
    QObject::connect(vr, &QPushButton::clicked, &window, [&] {
        lastFailure.clear(); preparationLog = false; logButton->setText("Open game log"); retry->hide(); logButton->hide();
        const bool tryAnyway = vrModeReadiness.state == pt::launcher::ReadinessState::Unknown;
        launchPending = true; play->setEnabled(false); vr->setEnabled(false); vrReadiness->setText("Starting the VR attempt…");
        const auto bundle = chosen;
        auto result = std::make_shared<pt::launcher::SessionState>();
        auto* launchThread = QThread::create([&, bundle, tryAnyway, result] {
            *result = pt::launcher::PlayVR(bundle, supported, tryAnyway);
            QMetaObject::invokeMethod(&window, [&, result] {
                launchPending = false;
                if (!result->error.isEmpty()) {
                    lastFailure = result->error; warning->setText(result->error); retry->show(); logButton->show();
                }
                refresh();
            }, Qt::QueuedConnection);
        });
        QObject::connect(launchThread, &QThread::finished, launchThread, &QObject::deleteLater);
        launchThread->start();
    });
    QObject::connect(retry, &QPushButton::clicked, &window, [&] {
        if (recoveryBlocked) { prepare({}, true); return; }
        cachedInspection.reset(); lastFailure.clear(); retry->hide(); refresh();
    });
    QObject::connect(retryVr, &QPushButton::clicked, &window, [&] {
        cachedInspection.reset(); refresh();
    });
    QObject::connect(resume, &QPushButton::clicked, &window, [&] {
        QString error; pt::launcher::ReturnToGame(pt::launcher::InspectSession(chosen), &error);
        if (!error.isEmpty()) warning->setText(error);
    });
    QObject::connect(settings, &QPushButton::clicked, &window, [&] {
#ifdef _WIN32
        constexpr auto platform = pt::launcher::Platform::Windows;
#else
        constexpr auto platform = pt::launcher::Platform::Linux;
#endif
        pt::launcher::SettingsDialog dialog(chosen, platform, supported, &window);
        dialog.exec();
    });
    QObject::connect(logButton, &QPushButton::clicked, &window, [&] {
        if (!chosen.empty()) QDesktopServices::openUrl(QUrl::fromLocalFile(preparationLog ?
            QString::fromStdU32String((chosen / ".pt-preparation.log").u32string()) : pt::launcher::InspectSession(chosen).log));
    });
    auto* monitor = new QTimer(&window); monitor->setInterval(1000);
    QObject::connect(monitor, &QTimer::timeout, &window, refresh); monitor->start();
    recoveryBlocked = !chosen.empty() && pt::launcher::PreparationNeedsRecovery(chosen);
    const bool hasImportArgument = argc == 2 && !QCoreApplication::arguments().at(1).startsWith('-');
    if (recoveryBlocked && !hasImportArgument) QTimer::singleShot(0, &window, [&] { prepare({}, true); });
    else refresh();
    if (hasImportArgument) {
        const QString source = QCoreApplication::arguments().at(1);
        if (!source.startsWith('-')) QTimer::singleShot(0, &window, [&, source] { prepare(source, false); });
    }
#ifdef PT_LAUNCHER_TEST_BUILD
    bool vrControlRetried = false;
    if (!chosen.empty()) QTimer::singleShot(300, &window, [&, testMode] {
        if (testMode == "--test-flat-unknown-control") {
            auto* readinessWait = new QTimer(&window); readinessWait->setInterval(10);
            QObject::connect(readinessWait, &QTimer::timeout, &window, [&, readinessWait] {
                if (readinessPending || !cachedInspection) return;
                readinessWait->stop();
                if (flatReadiness.state != pt::launcher::ReadinessState::Unknown ||
                    play->text() != "Try Play anyway" || !play->isEnabled()) std::_Exit(82);
                play->click();
                QTimer::singleShot(250, &window, [] { QApplication::quit(); });
            });
            readinessWait->start(); return;
        }
        if (testMode == "--test-vr-control") {
            auto* readinessWait = new QTimer(&window); readinessWait->setInterval(10);
            QObject::connect(readinessWait, &QTimer::timeout, &window, [&, readinessWait] {
                if (readinessPending || !cachedInspection) return;
                readinessWait->stop();
                if (vrModeReadiness.state != pt::launcher::ReadinessState::Unknown ||
                    vr->text() != "Try VR anyway" || !vr->isEnabled()) std::_Exit(81);
                if (!vrControlRetried) { vrControlRetried = true; retryVr->click(); readinessWait->start(); return; }
                vr->click();
                QTimer::singleShot(250, &window, [] { QApplication::quit(); });
            });
            readinessWait->start(); return;
        }
        if (testMode == "--test-preparation-prompt") {
            const QByteArray source = qgetenv("PT_LAUNCHER_TEST_SOURCE");
            if (source.isEmpty()) qFatal("Test preparation source is missing");
            prepare(QString::fromUtf8(source), false);
            confirmCancellation(false);
            if (!cancelPrompt || !cancelPrompt->defaultButton() || cancelPrompt->defaultButton()->text() != "Keep preparing") qFatal("Keep preparing is not the prompt default");
            QTimer::singleShot(50, &window, [&] {
                preparationPromptFocus = QApplication::focusWidget();
                if (!preparationPromptFocus) qFatal("Preparation prompt did not receive focus");
                if (preparationPromptFocus != cancelPrompt->defaultButton()) qFatal("Keep preparing is not the focused default action");
            });
            QTimer::singleShot(1500, &window, [&] {
                if (!preparationPromptProgressSeen || cancelPrompt) qFatal("Cancellation prompt was not dismissed after preparation completion");
                QApplication::quit();
            });
            return;
        }
        if (testMode == "--test-reconnect") {
            if (play->isEnabled() || !resume->isVisible()) std::abort();
            resume->click();
            if (warning->text().isEmpty()) std::abort();
            QApplication::quit(); return;
        }
        play->click(); play->click();
        if (testMode == "--test-start-failure") {
            if (!retry->isVisible() || !logButton->isVisible() || warning->text().isEmpty()) std::abort();
            QApplication::quit(); return;
        }
        QTimer::singleShot(100, &window, [testMode] {
            if (testMode == "--test-play-and-crash") std::_Exit(42);
            QApplication::quit();
        });
    });
#endif
    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setWidget(panel);
    layout->addWidget(scroll, 3);
    window.show();
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--screenshot") {
        const QString output = QString::fromLocal8Bit(argv[2]);
        QTimer::singleShot(250, &window, [&window, output] {
            window.grab().save(output);
            window.resize(480, 680);
            QTimer::singleShot(250, &window, [&window, output] {
                window.grab().save(output + ".narrow.png");
                QApplication::quit();
            });
        });
    }
    return app.exec();
}
