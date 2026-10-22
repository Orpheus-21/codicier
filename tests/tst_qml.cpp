#include "KeyMap.h"
#include "SpellChecker.h"
#include "Thesaurus.h"
#include "Workspaces.h"

#include <QAccessible>
#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QSettings>
#include <QStandardPaths>
#include <QtQuickTest>

// The tests import the same Codicier module as the app.
Q_IMPORT_QML_PLUGIN(CodicierPlugin)

// What a screen reader gets: the name and the role of the accessible
// interface of an item, not the QML attached values.
class ScreenReader : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static QString name(QObject *item)
    {
        QAccessibleInterface *face = QAccessible::queryAccessibleInterface(item);
        return face != nullptr ? face->text(QAccessible::Name) : QString();
    }
    Q_INVOKABLE static int role(QObject *item)
    {
        QAccessibleInterface *face = QAccessible::queryAccessibleInterface(item);
        return face != nullptr ? face->role() : -1;
    }
};

class Setup : public QObject
{
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty(QStringLiteral("screenReader"), &reader);
    }

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
        Thesaurus::searchPaths() = { QStringLiteral(FIXTURE_DIR "/thes") };
        QAccessible::setActive(true);
    }

private:
    ScreenReader reader;
};

// Runs every tst_*.qml file in QUICK_TEST_SOURCE_DIR.
QUICK_TEST_MAIN_WITH_SETUP(codicier, Setup)
#include "tst_qml.moc"
