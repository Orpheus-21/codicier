# The Phase 1 spike: tools that test the editor before the real screens exist.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(FIXTURE ${CMAKE_CURRENT_BINARY_DIR}/fixture.md)
add_custom_command(OUTPUT ${FIXTURE}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_fixture.py ${FIXTURE}
    DEPENDS gen_fixture.py)
add_custom_target(fixture ALL DEPENDS ${FIXTURE})

qt_add_executable(probe probe.cpp MarkdownHighlighter.cpp)
target_compile_options(probe PRIVATE -Wall -Wextra)
target_compile_definitions(probe PRIVATE
    PROBE_QML="${CMAKE_CURRENT_SOURCE_DIR}/EditorProbe.qml")
target_link_libraries(probe PRIVATE Qt6::Gui Qt6::Quick)
add_dependencies(probe fixture)

# The editor speed budget from CONSTRAINTS.md: p95 of 16 ms or less.
add_test(NAME check_editor_speed COMMAND probe ${FIXTURE} 16)
set_tests_properties(check_editor_speed PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# A manual check: type into the spike editor through the input method.
qt_add_executable(typecheck typecheck.cpp MarkdownHighlighter.cpp)
target_compile_options(typecheck PRIVATE -Wall -Wextra)
target_compile_definitions(typecheck PRIVATE
    PROBE_QML="${CMAKE_CURRENT_SOURCE_DIR}/EditorProbe.qml")
target_link_libraries(typecheck PRIVATE Qt6::Gui Qt6::Quick)
