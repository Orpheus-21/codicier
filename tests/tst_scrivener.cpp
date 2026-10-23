#include "ProjectStore.h"
#include "Scrivener.h"

#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestScrivener : public QObject
{
    Q_OBJECT

    const QString fixture = QStringLiteral(FIXTURE_DIR "/scriv/Lighthouse.scriv");

private slots:
    void readsTheBinderTree()
    {
        QString error;
        const std::optional<Scrivener::Binder> read = Scrivener::readBinder(fixture, &error);
        QVERIFY2(read, qPrintable(error));
        const Scrivener::Binder binder = read.value_or(Scrivener::Binder());
        QCOMPARE(binder.author, QStringLiteral("A. Writer"));
        QStringList types;
        for (const Scrivener::Item &root : binder.roots)
            types.append(root.type);
        QCOMPARE(types, (QStringList { "DraftFolder", "ResearchFolder", "Folder", "TrashFolder" }));

        const Scrivener::Item &part = binder.roots.at(0).children.at(0);
        QCOMPARE(part.title, QStringLiteral("Part One"));
        const Scrivener::Item &dusk = part.children.at(0);
        QCOMPARE(dusk.label, 1);
        QCOMPARE(dusk.status, 2);
        QCOMPARE(dusk.children.size(), 2);
        QCOMPARE(dusk.children.at(0).uuid, QStringLiteral("00000004-0000-4000-8000-000000000004"));
        QVERIFY(dusk.children.at(0).include);
        QVERIFY(!dusk.children.at(1).include);
        // A text with children, as Scrivener allows.
        const Scrivener::Item &morning = part.children.at(1);
        QCOMPARE(morning.type, QStringLiteral("Text"));
        QCOMPARE(morning.children.at(0).title, QStringLiteral("The log"));
        QCOMPARE(binder.roots.at(1).children.at(1).extension, QStringLiteral("png"));
    }

    void readsTheLabelsAndStatuses()
    {
        const std::optional<Scrivener::Binder> read = Scrivener::readBinder(fixture);
        QVERIFY(read);
        const Scrivener::Binder binder = read.value_or(Scrivener::Binder());
        QCOMPARE(binder.labels.value(1).name, QStringLiteral("Plot"));
        QVERIFY(qAbs(binder.labels.value(2).red - 0.952941) < 1e-6);
        QVERIFY(binder.labels.value(-1).red < 0);
        QCOMPARE(binder.statuses.value(3), QStringLiteral("Revised Draft"));
        QCOMPARE(binder.statuses.size(), 6);
    }

    void unknownTypesStay()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("Odd.scrivx"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(
            "<ScrivenerProject Version=\"2.0\"><Binder>"
            "<BinderItem UUID=\"A\" Type=\"DraftFolder\"><Title>Draft</Title><Children>"
            "<BinderItem UUID=\"B\" Type=\"WebArchive\"><Title>Saved page</Title></BinderItem>"
            "</Children></BinderItem></Binder><Future><Thing ID=\"1\"/></Future>"
            "</ScrivenerProject>");
        file.close();
        const std::optional<Scrivener::Binder> read = Scrivener::readBinder(dir.path());
        QVERIFY(read);
        QCOMPARE(read.value_or(Scrivener::Binder()).roots.at(0).children.at(0).type,
                 QStringLiteral("WebArchive"));
    }

    void importBuildsAProjectThatLoads()
    {
        QTemporaryDir dir;
        const QString target = dir.filePath("lighthouse");
        Scrivener::Skipped skipped;
        const QList<LoadError> errors
            = Scrivener::importProject(fixture, target, "Lighthouse", &skipped);
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.first().text()));
        const LoadResult result = ProjectStore::load(target);
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.errors.first().text()));
        const Project &project = result.project;
        QCOMPARE(project.title, QStringLiteral("Lighthouse"));
        QCOMPARE(project.data["author"].value_or(std::string()), std::string("A. Writer"));

        QStringList tree;
        for (const ProjectItem &item : project.items)
            tree.append(item.kind + ":" + item.title);
        QCOMPARE(tree,
                 (QStringList { "part:Part One", "chapter:Dusk", "scene:The stairs",
                                "scene:A cut scene", "chapter:Morning", "scene:Morning",
                                "scene:The log", "note:Imported from Scrivener", "folder:Research",
                                "note:Lighthouse facts", "file:Old map", "folder:Characters",
                                "note:Elin" }));
        QCOMPARE(project.items.at(0).section, QStringLiteral("manuscript"));
        QCOMPARE(project.items.at(8).section, QStringLiteral("notes"));
        QCOMPARE(project.items.at(10).section, QStringLiteral("research"));

        const ProjectItem &stairs = project.items.at(2);
        QFile text(QDir(target).filePath(stairs.file));
        QVERIFY(text.open(QIODevice::ReadOnly));
        QVERIFY(text.readAll().contains("*Fair. Wind north-west.*"));
        QCOMPARE(stairs.data["synopsis"].value_or(std::string()),
                 std::string("Elin climbs the stairs."));
        QCOMPARE(stairs.data["notes"].value_or(std::string()),
                 std::string("Check the step count."));
        QCOMPARE(stairs.data["label"].value_or(std::string()), std::string("orange"));
        QCOMPARE(stairs.data["status"].value_or(std::string()), std::string("revised"));
        QCOMPARE(project.items.at(1).data["label"].value_or(std::string()), std::string("blue"));
        QVERIFY(!project.items.at(3).data["include"].value_or(true));
        QCOMPARE(project.items.at(6).data["status"].value_or(std::string()), std::string("final"));
        const toml::array *tags = stairs.data["tags"].as_array();
        QVERIFY(tags != nullptr);
        QCOMPARE((*tags)[0].value_or(std::string()), std::string("label: Subplot"));
        QCOMPARE((*tags)[1].value_or(std::string()), std::string("status: Revised Draft"));
        QVERIFY(QFileInfo(QDir(target).filePath(project.items.at(10).file)).size() > 0);
        QVERIFY(project.items.at(10).file.endsWith(".png"));
        QCOMPARE(skipped.trash, 1);

        // The report lists what stayed out.
        QFile report(QDir(target).filePath(project.items.at(7).file));
        QVERIFY(report.open(QIODevice::ReadOnly));
        const QString note = QString::fromUtf8(report.readAll());
        QVERIFY2(note.contains("from Lighthouse.scriv"), qPrintable(note));
        QVERIFY(note.contains("- 1 item in the Trash\n"));
        QVERIFY(note.contains("- 1 snapshot\n"));
        QVERIFY(note.contains("- 1 text with linked comments or footnotes\n"));
        QVERIFY(note.contains("- 0 collections\n"));
        QVERIFY(note.contains("custom metadata"));
    }

    void importLeavesTheScrivFolderAlone()
    {
        QTemporaryDir dir;
        const auto listing = [this] {
            QStringList all;
            QDirIterator it(fixture, QDir::AllEntries | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                all.append(it.filePath() + " " + QString::number(it.fileInfo().size()));
            }
            all.sort();
            return all;
        };
        const QStringList before = listing();
        QVERIFY(Scrivener::importProject(fixture, dir.filePath("p"), "P").isEmpty());
        QCOMPARE(listing(), before);
        // A folder that is not empty is no place for an import.
        QVERIFY(!Scrivener::importProject(fixture, dir.filePath("p"), "P").isEmpty());
    }

    void brokenOrMissingBinderIsAnError()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!Scrivener::readBinder(dir.path(), &error));
        QVERIFY(error.contains("not a Scrivener project"));
        QFile file(dir.filePath("Broken.scrivx"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<ScrivenerProject><Binder><BinderItem UUID=\"A\">");
        file.close();
        QVERIFY(!Scrivener::readBinder(dir.path(), &error));
        QVERIFY2(error.contains("broken"), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestScrivener)
#include "tst_scrivener.moc"
