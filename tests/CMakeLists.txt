find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)

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
target_link_libraries(tst_qml PRIVATE Qt6::QuickTest)
add_test(NAME tst_qml COMMAND tst_qml)
set_tests_properties(tst_qml PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME check_offline
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/check_offline.sh ${PROJECT_SOURCE_DIR})
