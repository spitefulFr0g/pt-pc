#include "inspection.h"
#include <QApplication>
#include <QFileDialog>
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
    auto* play = new QPushButton("Play"); play->setEnabled(false); controls->addWidget(play);
    auto* vr = new QPushButton("Play in VR"); vr->setEnabled(false); controls->addWidget(vr);
    auto* settings = new QPushButton("Settings"); settings->setEnabled(false); controls->addWidget(settings);
    auto* files = new QPushButton("Game files"); controls->addWidget(files);
    auto* updates = new QPushButton("Updates"); updates->setEnabled(false); controls->addWidget(updates);
    auto* details = new QLabel; details->setWordWrap(true); controls->addWidget(details);
    QObject::connect(files, &QPushButton::clicked, &window, [&] {
        QString folder = QFileDialog::getExistingDirectory(&window, "Choose portable bundle");
        if (folder.isEmpty()) return;
        auto inspection = pt::launcher::InspectBundle(std::filesystem::path(folder.toStdU32String()));
        warning->setText(QString::fromStdString(inspection.error.empty() ?
            (inspection.linux_runtime.present || inspection.windows_runtime.present ?
              "Runtime unverified. Install a matching supported release to enable Play and Settings." :
              "Runtime missing. Add a matching platform runtime to this bundle.") : inspection.error));
        details->setText(QString("Archives: %1\nPath list: %2\nRetained source: %3\nSaves: %4\nMods: %5\nLinux: %6\nWindows: %7")
            .arg(inspection.archives ? "recognized" : "missing")
            .arg(inspection.path_list ? "recognized" : "missing")
            .arg(inspection.source ? "recognized" : "not found")
            .arg(inspection.saves ? "found" : "not found")
            .arg(inspection.mods ? "found" : "not found")
            .arg(inspection.linux_runtime.present ? "unverified" : "missing")
            .arg(inspection.windows_runtime.present ? "unverified" : "missing"));
    });
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
