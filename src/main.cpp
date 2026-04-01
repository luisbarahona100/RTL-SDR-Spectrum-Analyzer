#include <QApplication>
#include <QSurfaceFormat>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("RTL-SDR Spectrum Analyzer");
    app.setOrganizationName("NESDR");
    app.setStyle("Fusion");

    MainWindow win;
    win.show();

    return app.exec();
}
