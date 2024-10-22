#include <QApplication>
#include "StartupManager.hh"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    StartupManager window;
    window.show();
    return app.exec();
}
