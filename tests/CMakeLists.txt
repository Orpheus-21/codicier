# Only tst_offline uses Network: it counts network managers to prove the guard.
find_package(Qt6 REQUIRED COMPONENTS Network Test QuickTest)

# The tests need no display.
set(TEST_ENV "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_smoke tst_smoke.cpp)
target_compile_options(tst_smoke PRIVATE -Wall -Wextra)
target_link_libraries(tst_smoke PRIVATE Qt6::Qml Qt6::Quick Qt6::Test codicier_ui codicier_uiplugin)
add_test(NAME tst_smoke COMMAND tst_smoke)
set_tests_properties(tst_smoke PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_qml tst_qml.cpp)
target_compile_options(tst_qml PRIVATE -Wall -Wextra)
target_compile_definitions(tst_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_qml PRIVATE Qt6::Qml Qt6::QuickTest codicier_ui codicier_uiplugin)
add_test(NAME tst_qml COMMAND tst_qml)
set_tests_properties(tst_qml PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_offline tst_offline.cpp ${PROJECT_SOURCE_DIR}/src/OfflineGuard.cpp)
target_compile_options(tst_offline PRIVATE -Wall -Wextra)
target_include_directories(tst_offline PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_offline PRIVATE Qt6::Network Qt6::Qml Qt6::Quick Qt6::Test)
add_test(NAME tst_offline COMMAND tst_offline)
set_tests_properties(tst_offline PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_highlighter tst_highlighter.cpp
    ${PROJECT_SOURCE_DIR}/src/app/MarkdownHighlighter.cpp
    ${PROJECT_SOURCE_DIR}/src/app/SpellChecker.cpp)
target_compile_options(tst_highlighter PRIVATE -Wall -Wextra)
target_link_libraries(tst_highlighter PRIVATE PkgConfig::HUNSPELL)
target_include_directories(tst_highlighter PRIVATE ${PROJECT_SOURCE_DIR}/src/app)
target_compile_definitions(tst_highlighter PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_highlighter PRIVATE Qt6::Gui Qt6::Test)
add_test(NAME tst_highlighter COMMAND tst_highlighter)
set_tests_properties(tst_highlighter PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_projectstore tst_projectstore.cpp)
target_compile_options(tst_projectstore PRIVATE -Wall -Wextra)
target_compile_definitions(tst_projectstore PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_projectstore PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_projectstore COMMAND tst_projectstore)

qt_add_executable(tst_roundtrip tst_roundtrip.cpp)
target_compile_options(tst_roundtrip PRIVATE -Wall -Wextra)
target_compile_definitions(tst_roundtrip PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_roundtrip PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_roundtrip COMMAND tst_roundtrip)

qt_add_executable(tst_templates tst_templates.cpp)
target_compile_options(tst_templates PRIVATE -Wall -Wextra)
target_link_libraries(tst_templates PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_templates COMMAND tst_templates)

qt_add_executable(tst_index tst_index.cpp)
target_compile_options(tst_index PRIVATE -Wall -Wextra)
target_compile_definitions(tst_index PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_index PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_index COMMAND tst_index)

qt_add_executable(tst_watcher tst_watcher.cpp)
target_compile_options(tst_watcher PRIVATE -Wall -Wextra)
target_compile_definitions(tst_watcher PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_watcher PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_watcher COMMAND tst_watcher)

qt_add_executable(tst_workspaces tst_workspaces.cpp)
target_compile_options(tst_workspaces PRIVATE -Wall -Wextra)
target_link_libraries(tst_workspaces PRIVATE codicier_ui Qt6::Test)
add_test(NAME tst_workspaces COMMAND tst_workspaces)

qt_add_executable(tst_projectlist tst_projectlist.cpp)
target_compile_options(tst_projectlist PRIVATE -Wall -Wextra)
target_link_libraries(tst_projectlist PRIVATE codicier_ui Qt6::Test)
add_test(NAME tst_projectlist COMMAND tst_projectlist)

qt_add_executable(tst_projectsession tst_projectsession.cpp)
target_compile_options(tst_projectsession PRIVATE -Wall -Wextra)
target_compile_definitions(tst_projectsession PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_projectsession PRIVATE codicier_ui Qt6::Test)
add_test(NAME tst_projectsession COMMAND tst_projectsession)

qt_add_executable(tst_bindermodel tst_bindermodel.cpp)
target_compile_options(tst_bindermodel PRIVATE -Wall -Wextra)
target_compile_definitions(tst_bindermodel PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_bindermodel PRIVATE codicier_ui Qt6::Test)
add_test(NAME tst_bindermodel COMMAND tst_bindermodel)

qt_add_executable(tst_document tst_document.cpp)
target_compile_options(tst_document PRIVATE -Wall -Wextra)
target_compile_definitions(tst_document PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_document PRIVATE codicier_ui Qt6::Test)
add_test(NAME tst_document COMMAND tst_document)
set_tests_properties(tst_document PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_binderedit tst_binderedit.cpp)
target_compile_options(tst_binderedit PRIVATE -Wall -Wextra)
target_compile_definitions(tst_binderedit PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_binderedit PRIVATE codicier_ui Qt6::Test)
add_test(NAME tst_binderedit COMMAND tst_binderedit)

qt_add_executable(tst_snapshots tst_snapshots.cpp)
target_compile_options(tst_snapshots PRIVATE -Wall -Wextra)
target_compile_definitions(tst_snapshots PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_snapshots PRIVATE codicier_ui Qt6::Test)
add_test(NAME tst_snapshots COMMAND tst_snapshots)
set_tests_properties(tst_snapshots PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_diff tst_diff.cpp)
target_compile_options(tst_diff PRIVATE -Wall -Wextra)
target_link_libraries(tst_diff PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_diff COMMAND tst_diff)

qt_add_executable(tst_backup tst_backup.cpp)
target_compile_options(tst_backup PRIVATE -Wall -Wextra)
target_compile_definitions(tst_backup PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_backup PRIVATE codicier_core Qt6::CorePrivate Qt6::Test)
add_test(NAME tst_backup COMMAND tst_backup)

qt_add_executable(tst_versions tst_versions.cpp)
target_compile_options(tst_versions PRIVATE -Wall -Wextra)
target_link_libraries(tst_versions PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_versions COMMAND tst_versions)

qt_add_executable(tst_calendar tst_calendar.cpp)
target_compile_options(tst_calendar PRIVATE -Wall -Wextra)
target_link_libraries(tst_calendar PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_calendar COMMAND tst_calendar)

qt_add_executable(tst_history tst_history.cpp)
target_compile_options(tst_history PRIVATE -Wall -Wextra)
target_link_libraries(tst_history PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_history COMMAND tst_history)

qt_add_executable(tst_sources tst_sources.cpp)
target_compile_options(tst_sources PRIVATE -Wall -Wextra)
target_link_libraries(tst_sources PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_sources COMMAND tst_sources)

qt_add_executable(tst_settings tst_settings.cpp)
target_compile_options(tst_settings PRIVATE -Wall -Wextra)
target_link_libraries(tst_settings PRIVATE codicier_ui Qt6::Test)
add_test(NAME tst_settings COMMAND tst_settings)

add_test(NAME check_offline
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/check_offline.sh ${PROJECT_SOURCE_DIR})

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME tst_floor_guard
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tst_floor_guard.py)
add_test(NAME tst_coverage_diff
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tst_coverage_diff.py)
add_test(NAME tst_icons
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tst_icons.py)
# qml/IconPaths.js must match the SVG files in icons/.
add_test(NAME check_icons
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/icons.py --check)

# Lint every QML file. One warning fails the test.
# The module files use the arguments that Qt generates for the module. They
# map each source file into the Codicier module, so qmllint reads its qmldir.
add_test(NAME check_qmllint
    COMMAND $<TARGET_FILE:Qt6::qmllint> --max-warnings 0
    @${CMAKE_BINARY_DIR}/.rcc/qmllint/codicier_ui.rsp)
file(GLOB_RECURSE QML_LINT_FILES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/spike/*.qml ${PROJECT_SOURCE_DIR}/mocks/*.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml)
# The same arguments map the module files into the module, so qmllint also
# finds the C++ types of the module for the files that use it.
add_test(NAME check_qmllint_tests
    COMMAND $<TARGET_FILE:Qt6::qmllint> --max-warnings 0
    @${CMAKE_BINARY_DIR}/.rcc/qmllint/codicier_ui.rsp ${QML_LINT_FILES})

# Check the C++ format against .clang-format. The Linux CI has clang-format,
# so the check runs there; a build on Windows or macOS can be without it.
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE FORMAT_FILES CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.h ${PROJECT_SOURCE_DIR}/spike/*.cpp
        ${PROJECT_SOURCE_DIR}/mocks/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
    add_test(NAME check_format
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${FORMAT_FILES})
endif()

qt_add_executable(tst_exporter tst_exporter.cpp)
target_compile_options(tst_exporter PRIVATE -Wall -Wextra)
target_link_libraries(tst_exporter PRIVATE codicier_ui Qt6::Test Qt6::CorePrivate)
add_test(NAME tst_exporter COMMAND tst_exporter)
set_tests_properties(tst_exporter PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_scrivener tst_scrivener.cpp)
target_compile_options(tst_scrivener PRIVATE -Wall -Wextra)
target_compile_definitions(tst_scrivener PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_scrivener PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_scrivener COMMAND tst_scrivener)

qt_add_executable(tst_rtf tst_rtf.cpp)
target_compile_options(tst_rtf PRIVATE -Wall -Wextra)
target_compile_definitions(tst_rtf PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_link_libraries(tst_rtf PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_rtf COMMAND tst_rtf)

qt_add_executable(tst_markdownimport tst_markdownimport.cpp)
target_compile_options(tst_markdownimport PRIVATE -Wall -Wextra)
target_link_libraries(tst_markdownimport PRIVATE codicier_core Qt6::Test)
add_test(NAME tst_markdownimport COMMAND tst_markdownimport)
