cmake_minimum_required(VERSION 3.21)

project(codicier VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# clang-tidy and editors read build/compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CorePrivate gives QZipWriter for backups; it ships with qt6-base. The use is
# on purpose (see src/model/Backup.cpp), so the notice about it is off.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 6.12 REQUIRED COMPONENTS Gui Qml Quick Sql CorePrivate)
# Hunspell checks the spelling, offline, with the dictionaries of the system.
find_package(PkgConfig REQUIRED)
pkg_check_modules(HUNSPELL REQUIRED IMPORTED_TARGET hunspell)
# toml++ reads the project files (docs/format.md).
find_package(tomlplusplus 3 REQUIRED)
qt_standard_project_setup(REQUIRES 6.12)

# The project model: plain C++ and Qt Core, with no UI.
add_library(codicier_core STATIC src/model/ProjectStore.cpp src/model/TomlWriter.cpp
    src/model/Naming.cpp src/model/Templates.cpp src/model/Watcher.cpp src/model/Snapshots.cpp
    src/model/Diff.cpp src/model/Backup.cpp src/model/History.cpp src/model/Versions.cpp src/model/Frameworks.cpp src/model/Calendar.cpp
    src/index/Index.cpp)
target_include_directories(codicier_core PUBLIC src/model src/index)
# The plot frameworks of the beat board.
qt_add_resources(codicier_core "data" PREFIX "/" FILES data/frameworks.toml)
target_compile_options(codicier_core PRIVATE -Wall -Wextra)
target_link_libraries(codicier_core PUBLIC Qt6::Core Qt6::Sql PRIVATE Qt6::CorePrivate)
# Arch builds toml++ as a shared library. Use only its headers, in header-only
# mode, so the app needs no toml++ library at run time.
get_target_property(TOML_INCLUDE tomlplusplus::tomlplusplus INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(codicier_core SYSTEM PUBLIC ${TOML_INCLUDE})
target_compile_definitions(codicier_core PUBLIC TOML_HEADER_ONLY=1)

# The QML module is a static library, so the app and the tests link the same module.
# Its files go to build/qml/Codicier, so qmllint and tools find them by import path.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
qt_add_library(codicier_ui STATIC)
target_include_directories(codicier_ui PUBLIC src/app)
target_link_libraries(codicier_ui PUBLIC codicier_core Qt6::Gui Qt6::Quick PRIVATE PkgConfig::HUNSPELL)
target_compile_options(codicier_ui PRIVATE -Wall -Wextra)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(codicier_ui
    URI Codicier
    QML_FILES qml/Main.qml qml/Theme.qml qml/Icon.qml qml/IconPaths.js
        qml/FocusRing.qml qml/Button.qml qml/IconButton.qml qml/TextField.qml
        qml/ListItem.qml qml/Pane.qml qml/Splitter.qml qml/Menu.qml
        qml/WorkspaceHome.qml qml/NewProjectDialog.qml qml/ProjectView.qml
        qml/Binder.qml qml/BinderRow.qml qml/Editor.qml qml/TextArea.qml qml/Inspector.qml
        qml/PageText.qml qml/Composite.qml qml/CompositeBlock.qml
        qml/CompareView.qml qml/SettingsDialog.qml qml/SearchPanel.qml qml/CommandPalette.qml qml/Choice.qml qml/FontPicker.qml qml/DeleteProjectDialog.qml qml/CodexPanel.qml qml/TypeDialog.qml qml/Corkboard.qml qml/Outliner.qml qml/ProgressDialog.qml qml/VersionDialog.qml qml/VersionsPanel.qml qml/BeatBoard.qml qml/PlotGrid.qml qml/Timeline.qml qml/CalendarDialog.qml qml/RelationGraph.qml qml/FamilyTree.qml qml/MapView.qml qml/Insights.qml qml/PassDialog.qml qml/CommentDialog.qml qml/InboxPanel.qml qml/Checklist.qml
    SOURCES src/app/Workspaces.cpp src/app/Workspaces.h
        src/app/ProjectList.cpp src/app/ProjectList.h
        src/app/ProjectSession.cpp src/app/ProjectSession.h
        src/app/BinderModel.cpp src/app/BinderModel.h
        src/app/Document.cpp src/app/Document.h
        src/app/MarkdownHighlighter.cpp src/app/MarkdownHighlighter.h
        src/app/Settings.cpp src/app/Settings.h
        src/app/KeyMap.cpp src/app/KeyMap.h src/app/SpellChecker.cpp src/app/SpellChecker.h
    RESOURCES fonts/Literata.ttf fonts/Literata-Italic.ttf fonts/Inter.ttf
)

qt_add_executable(codicier src/main.cpp src/OfflineGuard.cpp)
target_compile_options(codicier PRIVATE -Wall -Wextra)
target_link_libraries(codicier PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick codicier_uiplugin)
if(APPLE)
    # The icon holds one 256 px PNG (made from packaging/linux/codicier.png).
    target_sources(codicier PRIVATE packaging/macos/codicier.icns)
    set_source_files_properties(packaging/macos/codicier.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(codicier PROPERTIES MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME Codicier
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.Orpheus_21.Codicier
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE codicier.icns)
endif()

include(GNUInstallDirs)
install(TARGETS codicier RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
install(FILES packaging/linux/codicier.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/linux/codicier.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
install(FILES packaging/linux/codicier.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)

enable_testing()
add_subdirectory(tests)
add_subdirectory(spike)
add_subdirectory(mocks)
