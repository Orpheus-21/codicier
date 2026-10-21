#include "KeyMap.h"
#include "SpellChecker.h"
#include "Workspaces.h"

#include <QFile>
#include <QQmlExtensionPlugin>
#include <QSettings>
#include <QStandardPaths>
#include <QtQuickTest>

// The tests import the same Codicier module as the app.
Q_IMPORT_QML_PLUGIN(CodicierPlugin)

class Setup : public QObject
{
    Q_OBJECT

public slots:
    // The tests must not read or write the settings of the author.
    void applicationAvailable()
    {
        QStandardPaths::setTestModeEnabled(true);
        QFile::remove(Workspaces::settingsPath());
        QFile::remove(KeyMap().file());
        // The first-run hints take Escape, so most tests start without them.
        QSettings(Workspaces::settingsPath(), QSettings::IniFormat)
            .setValue("view/hintsSeen", true);
        // The tests use the small dictionary of the fixtures, not the system.
        SpellChecker::searchPaths() = { QStringLiteral(FIXTURE_DIR "/dict") };
    }
};

// Runs every tst_*.qml file in QUICK_TEST_SOURCE_DIR.
QUICK_TEST_MAIN_WITH_SETUP(codicier, Setup)
#include "tst_qml.moc"
