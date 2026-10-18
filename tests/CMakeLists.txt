# Only tst_offline uses Network: it counts network managers to prove the guard.
find_package(Qt6 REQUIRED COMPONENTS Network Test QuickTest)

# The tests need no display.
set(TEST_ENV "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_smoke tst_smoke.cpp)
target_compile_options(tst_smoke PRIVATE -Wall -Wextra)
target_compile_definitions(tst_smoke PRIVATE
    MAIN_QML="${PROJECT_SOURCE_DIR}/qml/Main.qml")
target_link_libraries(tst_smoke PRIVATE Qt6::Qml Qt6::Quick Qt6::Test)
add_test(NAME tst_smoke COMMAND tst_smoke)
set_tests_properties(tst_smoke PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_qml tst_qml.cpp)
target_compile_options(tst_qml PRIVATE -Wall -Wextra)
target_compile_definitions(tst_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
target_link_libraries(tst_qml PRIVATE Qt6::Qml Qt6::QuickTest codicier_uiplugin)
add_test(NAME tst_qml COMMAND tst_qml)
set_tests_properties(tst_qml PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_offline tst_offline.cpp ${PROJECT_SOURCE_DIR}/src/OfflineGuard.cpp)
target_compile_options(tst_offline PRIVATE -Wall -Wextra)
target_include_directories(tst_offline PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tst_offline PRIVATE Qt6::Network Qt6::Qml Qt6::Quick Qt6::Test)
add_test(NAME tst_offline COMMAND tst_offline)
set_tests_properties(tst_offline PROPERTIES ENVIRONMENT "${TEST_ENV}")

qt_add_executable(tst_highlighter tst_highlighter.cpp
    ${PROJECT_SOURCE_DIR}/spike/MarkdownHighlighter.cpp)
target_compile_options(tst_highlighter PRIVATE -Wall -Wextra)
target_include_directories(tst_highlighter PRIVATE ${PROJECT_SOURCE_DIR}/spike)
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
add_test(NAME check_qmllint_tests
    COMMAND $<TARGET_FILE:Qt6::qmllint> --max-warnings 0 -I ${CMAKE_BINARY_DIR}/qml
    ${QML_LINT_FILES})

# Check the C++ format against .clang-format.
find_program(CLANG_FORMAT clang-format REQUIRED)
file(GLOB_RECURSE FORMAT_FILES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.h ${PROJECT_SOURCE_DIR}/spike/*.cpp
    ${PROJECT_SOURCE_DIR}/mocks/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_test(NAME check_format
    COMMAND ${CLANG_FORMAT} --dry-run --Werror ${FORMAT_FILES})
