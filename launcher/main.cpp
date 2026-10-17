#include "inspection.h"
#include "session.h"
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
#include <algorithm>
#include <cstdlib>
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
protected:
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
    if (QString(PT_SUPPORTED_LINUX_SHA256).size() == 64) supported.push_back({"linux", PT_SUPPORTED_LINUX_SHA256, "bundled"});
    if (QString(PT_SUPPORTED_WINDOWS_SHA256).size() == 64) supported.push_back({"windows", PT_SUPPORTED_WINDOWS_SHA256, "bundled"});
    std::filesystem::path chosen;
    std::optional<pt::launcher::BundleInspection> cachedInspection;
    QString lastFailure;
    bool previousRunning = false;
    QSettings preferences("pt-pc", "pt-launcher");
    const QString lastBundle = preferences.value("lastBundle").toString();
    if (!lastBundle.isEmpty()) chosen = std::filesystem::path(lastBundle.toStdU32String());
#ifdef PT_LAUNCHER_TEST_BUILD
    QString testMode;
    if (argc == 5 && QString::fromLocal8Bit(argv[1]).startsWith("--test-") && QString::fromLocal8Bit(argv[3]) == "--test-supported-sha256") {
        testMode = QString::fromLocal8Bit(argv[1]);
        chosen = std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdU32String());
        supported.push_back({"linux", argv[4], "fixture"});
    }
#endif
    const QString assets = QCoreApplication::applicationDirPath() + "/assets/";
    QFontDatabase::addApplicationFont(assets + "PirataOne-Regular.ttf");
    ResponsiveWindow window; window.setWindowTitle("P.T. Launcher"); window.resize(1000, 680);
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
    auto* warning = new QLabel("Choose a portable bundle in Game files."); warning->setObjectName("warning"); warning->setWordWrap(true); controls->addWidget(warning);
    auto* play = new QPushButton("Play"); play->setObjectName("play"); play->setEnabled(false); controls->addWidget(play);
    auto* resume = new QPushButton("Return to game"); resume->setObjectName("returnToGame"); resume->hide(); controls->addWidget(resume);
    auto* retry = new QPushButton("Retry"); retry->hide(); controls->addWidget(retry);
    auto* logButton = new QPushButton("Open game log"); logButton->hide(); controls->addWidget(logButton);
    auto* vr = new QPushButton("Play in VR"); vr->setEnabled(false); controls->addWidget(vr);
    auto* settings = new QPushButton("Settings"); settings->setEnabled(false); controls->addWidget(settings);
    auto* files = new QPushButton("Game files"); controls->addWidget(files);
    auto* updates = new QPushButton("Updates"); updates->setEnabled(false); controls->addWidget(updates);
    auto* details = new QLabel; details->setWordWrap(true); controls->addWidget(details);
    auto refresh = [&] {
        if (chosen.empty()) return;
        auto session = pt::launcher::InspectSession(chosen);
        if (previousRunning && session.activity == pt::launcher::Activity::Stopped) {
            lastFailure = "Game process exited. Open the game log or retry Play.";
            retry->show(); logButton->show();
            cachedInspection.reset();
        }
        if (!cachedInspection) cachedInspection = pt::launcher::InspectBundle(chosen, supported);
        const auto& inspection = *cachedInspection;
        previousRunning = session.activity == pt::launcher::Activity::Running;
        const bool runtime =
#ifdef _WIN32
            inspection.windows_runtime.verified;
#else
            inspection.linux_runtime.verified;
#endif
        play->setEnabled(session.activity == pt::launcher::Activity::Stopped && inspection.archives && runtime);
        resume->setVisible(session.activity == pt::launcher::Activity::Running);
        if (session.activity == pt::launcher::Activity::Running) warning->setText("Game process active. Return to the existing session.");
        else if (session.activity == pt::launcher::Activity::Uncertain) warning->setText(session.error.isEmpty() ? "Game activity uncertain. Play is blocked." : session.error);
        else if (!lastFailure.isEmpty()) warning->setText(lastFailure);
        else warning->setText(QString::fromStdString(inspection.error.empty() ?
            (runtime ? "" : inspection.linux_runtime.present || inspection.windows_runtime.present ?
              "Runtime unverified. Install a matching supported release to enable Play and Settings." :
              "Runtime missing. Add a matching platform runtime to this bundle.") : inspection.error));
        details->setText(QString("Archives: %1\nPath list: %2\nRetained source: %3\nSaves: %4\nMods: %5\nLinux: %6\nWindows: %7")
            .arg(inspection.archives ? "recognized" : "missing")
            .arg(inspection.path_list ? "recognized" : "missing")
            .arg(inspection.source ? "recognized" : "not found")
            .arg(inspection.saves ? "found" : "not found")
            .arg(inspection.mods ? "found" : "not found")
            .arg(inspection.linux_runtime.verified ? "verified" : inspection.linux_runtime.present ? "unverified" : "missing")
            .arg(inspection.windows_runtime.verified ? "verified" : inspection.windows_runtime.present ? "unverified" : "missing"));
    };
    QObject::connect(files, &QPushButton::clicked, &window, [&] {
        QString folder = QFileDialog::getExistingDirectory(&window, "Choose portable bundle");
        if (folder.isEmpty()) return;
        chosen = std::filesystem::path(folder.toStdU32String()); cachedInspection.reset(); lastFailure.clear(); previousRunning = false; refresh();
        preferences.setValue("lastBundle", folder);
    });
    QObject::connect(play, &QPushButton::clicked, &window, [&] {
        lastFailure.clear(); retry->hide(); logButton->hide();
        auto result = pt::launcher::PlayFlat(chosen, supported);
        refresh();
        if (!result.error.isEmpty()) { lastFailure = result.error; warning->setText(result.error); retry->show(); logButton->show(); }
    });
    QObject::connect(retry, &QPushButton::clicked, &window, [&] { cachedInspection.reset(); lastFailure.clear(); retry->hide(); refresh(); });
    QObject::connect(resume, &QPushButton::clicked, &window, [&] {
        QString error; pt::launcher::ReturnToGame(pt::launcher::InspectSession(chosen), &error);
        if (!error.isEmpty()) warning->setText(error);
    });
    QObject::connect(logButton, &QPushButton::clicked, &window, [&] {
        if (!chosen.empty()) QDesktopServices::openUrl(QUrl::fromLocalFile(pt::launcher::InspectSession(chosen).log));
    });
    auto* monitor = new QTimer(&window); monitor->setInterval(1000);
    QObject::connect(monitor, &QTimer::timeout, &window, refresh); monitor->start();
    refresh();
#ifdef PT_LAUNCHER_TEST_BUILD
    if (!chosen.empty()) QTimer::singleShot(0, &window, [&, testMode] {
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
