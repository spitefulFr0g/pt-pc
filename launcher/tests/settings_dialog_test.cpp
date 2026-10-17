#include "settings_fixture.h"
#include "settings_dialog.h"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs=std::filesystem;
static void put(const fs::path& p,const std::string& s){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary)<<s;}
static std::string get(const fs::path& p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char** argv){
    QApplication app(argc,argv); QTemporaryDir temp; assert(temp.isValid());
    const fs::path root=fs::path(temp.path().toStdU32String())/"bundle with spaces";
    const auto supported=SettingsRuntimes(root);
    const fs::path ini=root/"linux/data/pt.ini";
    put(ini,"; retain\n[display]\nwidth = 1280 ; retain inline note\nheight = 720\nfullscreen = 0\nunknown = keep\n");
    { pt::launcher::SettingsDialog unverified(root,pt::launcher::Platform::Linux,{}); assert(!unverified.loaded()); }
    pt::launcher::SettingsDialog dialog(root,pt::launcher::Platform::Linux,supported);
    assert(dialog.loaded());
    auto* width=dialog.findChild<QSpinBox*>("displayWidth");
    auto* mode=dialog.findChild<QComboBox*>("displayWindowMode");
    auto* apply=dialog.findChild<QPushButton*>("settingsApply");
    auto* discard=dialog.findChild<QPushButton*>("settingsDiscard");
    auto* box=dialog.findChild<QDialogButtonBox*>();
    assert(width&&mode&&apply&&discard&&box);
    dialog.show();
    width->setValue(1600); apply->click();
    assert(get(ini).find("width = 1600")!=std::string::npos);
    assert(get(ini).find("width = 1600 ; retain inline note")!=std::string::npos);
    mode->setCurrentIndex(2); apply->click();
    assert(mode->currentIndex()==2&&get(ini).find("fullscreen = 2")!=std::string::npos);
    mode->setCurrentIndex(1); discard->click();
    assert(mode->currentIndex()==2&&get(ini).find("fullscreen = 2")!=std::string::npos);
    width->setValue(1680);
    put(ini,"[display]\nwidth=1366\nheight=810\nfullscreen=1\nunknown=external\n");
    QTimer::singleShot(0,&dialog,[&]{
        for(auto* w: QApplication::topLevelWidgets()) if(auto* m=qobject_cast<QMessageBox*>(w)) {
            for(auto* b:m->buttons()) if(b->text()=="Reconcile my edits") { b->click(); return; }
        }
        std::abort();
    });
    apply->click();
    assert(dialog.statusText().contains("Apply again"));
    assert(get(ini).find("width=1366")!=std::string::npos);
    apply->click();
    assert(get(ini).find("width=1680")!=std::string::npos);
    assert(get(ini).find("unknown=external")!=std::string::npos);
    assert(get(ini).find("height=810\nfullscreen=1")!=std::string::npos);
    put(ini,"[display]\nwidth=1366\nheight=768\nfullscreen=1\nunknown=reload\n");
    auto* reload=dialog.findChild<QPushButton*>("settingsReload");
    assert(reload);
    reload->click();
    assert(width->value()==1366&&mode->currentIndex()==1);
    assert(dialog.statusText().contains("reloaded"));
    width->setValue(1920);
    QTimer::singleShot(0,&dialog,[&]{
        for(auto* w: QApplication::topLevelWidgets()) if(auto* m=qobject_cast<QMessageBox*>(w)) {
            for(auto* b:m->buttons()) if(b->text()=="Keep editing") { b->click(); return; }
        }
        std::abort();
    });
    box->button(QDialogButtonBox::Close)->click();
    assert(dialog.isVisible()&&width->value()==1920);
    QTimer::singleShot(0,&dialog,[&]{
        for(auto* w: QApplication::topLevelWidgets()) if(auto* m=qobject_cast<QMessageBox*>(w)) {
            for(auto* b:m->buttons()) if(b->text()=="Keep editing") { b->click(); return; }
        }
        std::abort();
    });
    dialog.reject();
    assert(dialog.isVisible());
    dialog.hide();
    {
        pt::launcher::SettingsDialog replaced(root,pt::launcher::Platform::Linux,supported);
        assert(replaced.loaded());
        const auto before=get(ini);
        put(root/"linux/pt","replacement runtime");
        replaced.findChild<QSpinBox*>("displayWidth")->setValue(1800);
        replaced.findChild<QPushButton*>("settingsApply")->click();
        assert(replaced.statusText().contains("Runtime unverified"));
        assert(get(ini)==before);
        SettingsRuntimes(root);
    }

#ifdef __linux__
    const auto executable=root/"linux/pt";
    fs::copy_file("/bin/sleep",executable,fs::copy_options::overwrite_existing);
    fs::permissions(executable,fs::perms::owner_exec,fs::perm_options::add);
    pt::launcher::SettingsDialog afterExit(root,pt::launcher::Platform::Linux,supported);
    afterExit.show();
    qint64 pid=0;
    assert(QProcess::startDetached(QString::fromStdU32String(executable.u32string()),{"20"},{},&pid));
    for(int i=0;i<100&&pt::launcher::InspectSession(root).activity!=pt::launcher::Activity::Running;++i) QThread::msleep(10);
    QThread::msleep(650); app.processEvents();
    put(ini,"[display]\nwidth=1750\nheight=910\nfullscreen=1\n");
    QProcess::execute("kill",{"-TERM",QString::number(pid)});
    for(int i=0;i<100&&pt::launcher::InspectSession(root).activity==pt::launcher::Activity::Running;++i) QThread::msleep(10);
    for(int i=0;i<100&&afterExit.findChild<QSpinBox*>("displayWidth")->value()!=1750;++i) { app.processEvents(); QThread::msleep(10); }
    assert(afterExit.findChild<QSpinBox*>("displayWidth")->value()==1750);
    assert(afterExit.statusText().contains("reloaded from the runtime"));
    afterExit.hide();
#endif
}
