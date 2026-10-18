#include "BinderModel.h"
#include "ProjectStore.h"

#include <QTest>

namespace {

// Each visible row as "depth:title", with "#" before a section heading.
QStringList outline(const BinderModel &model)
{
    QStringList rows;
    for (int i = 0; i < model.rowCount(); ++i) {
        const QModelIndex index = model.index(i);
        const QString title = model.data(index, BinderModel::TitleRole).toString();
        if (model.data(index, BinderModel::IsSectionRole).toBool())
            rows.append("#" + title);
        else
            rows.append(QString::number(model.data(index, BinderModel::DepthRole).toInt()) + ":"
                        + title);
    }
    return rows;
}

} // namespace

class TestBinderModel : public QObject
{
    Q_OBJECT

    BinderModel model;

private slots:
    void init()
    {
        const LoadResult result = ProjectStore::load(QStringLiteral(FIXTURE_DIR "/novel"));
        QVERIFY(result.ok());
        // A different project first, so each test starts with the default
        // open and closed state.
        model.setProject(Project());
        model.setProject(result.project);
    }

    void showsTheTreeInOrderWithSections()
    {
        QCOMPARE(outline(model),
                 (QStringList { "#Front matter", "0:Dedication", "#Manuscript", "0:Part One",
                                "1:The Letter", "2:The River Office", "2:A Date She Did Not Know",
                                "0:Part Two", "#Notes", "0:The ledger", "#Research",
                                "0:Map of the river", "#Trash" }));
    }

    void rowsCarryTheItemData()
    {
        const QModelIndex scene = model.index(model.rowOf("t6v4h9r2mz"));
        QCOMPARE(model.data(scene, BinderModel::KindRole).toString(), QStringLiteral("scene"));
        QCOMPARE(model.data(scene, BinderModel::StatusRole).toString(), QStringLiteral("revised"));
        QCOMPARE(model.data(scene, BinderModel::WordsRole).toInt(), 18);
        QCOMPARE(model.data(scene, BinderModel::HasChildrenRole).toBool(), false);
        QCOMPARE(model.data(scene, BinderModel::MissingRole).toBool(), false);

        // A chapter counts the words of its scenes.
        const QModelIndex chapter = model.index(model.rowOf("f6g7h8j9k0"));
        QCOMPARE(model.data(chapter, BinderModel::WordsRole).toInt(), 10 + 18);
        QCOMPARE(model.data(chapter, BinderModel::HasChildrenRole).toBool(), true);
        QCOMPARE(model.data(chapter, BinderModel::ExpandedRole).toBool(), true);
        // A research file has no word count, and a page has no status.
        QCOMPARE(model.data(model.index(model.rowOf("y0z1a2b3c4")), BinderModel::WordsRole).toInt(),
                 0);
        QCOMPARE(
            model.data(model.index(model.rowOf("d4f7k2m9pq")), BinderModel::StatusRole).toString(),
            QString());
    }

    void toggleClosesAndOpensABranch()
    {
        const int part = model.rowOf("a1b2c3d4e5");
        model.toggle(part);
        QVERIFY(!outline(model).contains("1:The Letter"));
        QCOMPARE(model.rowOf("t6v4h9r2mz"), -1);
        QCOMPARE(model.data(model.index(part), BinderModel::ExpandedRole).toBool(), false);
        model.setExpanded(part, true);
        QVERIFY(outline(model).contains("1:The Letter"));
        // Setting the same state again changes nothing.
        model.setExpanded(part, true);
        QCOMPARE(outline(model).size(), 13);
    }

    void trashOpensFromItsHeading()
    {
        const int trash = model.rowOf("section:trash");
        QCOMPARE(model.data(model.index(trash), BinderModel::ExpandedRole).toBool(), false);
        model.toggle(trash);
        QCOMPARE(outline(model).last(), QStringLiteral("0:Old opening"));
        QCOMPARE(model.data(model.index(trash), BinderModel::IdRole).toString(),
                 QStringLiteral("section:trash"));
        QCOMPARE(model.data(model.index(trash), BinderModel::KindRole).toString(),
                 QStringLiteral("section"));
    }

    void badRowsGiveNothing()
    {
        model.toggle(-1);
        model.toggle(999);
        model.setExpanded(999, false);
        QCOMPARE(outline(model).size(), 13);
        QVERIFY(!model.data(model.index(999), BinderModel::TitleRole).isValid());
        QVERIFY(!model.data(model.index(0), Qt::DisplayRole).isValid());
        QVERIFY(!model.data(model.index(1), Qt::DisplayRole).isValid());
        QCOMPARE(model.rowCount(model.index(0)), 0);
        QCOMPARE(model.rowOf("nothing"), -1);
        QCOMPARE(model.roleNames().size(), 10);
    }

    void openStateSurvivesAChangeInTheSameProject()
    {
        const LoadResult result = ProjectStore::load(QStringLiteral(FIXTURE_DIR "/novel"));
        model.toggle(model.rowOf("a1b2c3d4e5"));
        model.setProject(result.project);
        QCOMPARE(model.rowOf("t6v4h9r2mz"), -1);
    }

    void sectionLabels()
    {
        QCOMPARE(BinderModel::sectionLabel("back"), QStringLiteral("Back matter"));
        QCOMPARE(BinderModel::sectionLabel("research"), QStringLiteral("Research"));
        QCOMPARE(BinderModel::sectionLabel(""), QString());
    }
};

QTEST_GUILESS_MAIN(TestBinderModel)
#include "tst_bindermodel.moc"
