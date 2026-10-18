#include <QQmlExtensionPlugin>
#include <QtQuickTest>

// The tests import the same Codicier module as the app.
Q_IMPORT_QML_PLUGIN(CodicierPlugin)

// Runs every tst_*.qml file in QUICK_TEST_SOURCE_DIR.
QUICK_TEST_MAIN(codicier)
