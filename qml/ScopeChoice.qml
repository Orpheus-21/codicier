import QtQuick

// The scope of the corkboard and the outliner: the open chapter, or every
// scene of the book.
Row {
    id: scope

    required property bool wholeBook
    signal chosen(bool whole)

    spacing: Theme.space.xs

    Choice {
        objectName: "scopeChapter"
        label: "This chapter"
        chosen: !scope.wholeBook
        onClicked: scope.chosen(false)
    }

    Choice {
        objectName: "scopeBook"
        label: "Whole book"
        chosen: scope.wholeBook
        onClicked: scope.chosen(true)
    }
}
