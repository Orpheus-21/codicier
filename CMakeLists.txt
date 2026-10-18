cmake_minimum_required(VERSION 3.21)

project(codicier VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# clang-tidy and editors read build/compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)

# The QML module is a static library, so the app and the tests link the same module.
# Its files go to build/qml/Codicier, so qmllint and tools find them by import path.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
qt_add_library(codicier_ui STATIC)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(codicier_ui
    URI Codicier
    QML_FILES qml/Main.qml qml/Theme.qml qml/Icon.qml qml/IconPaths.js
    RESOURCES fonts/Literata.ttf fonts/Literata-Italic.ttf fonts/Inter.ttf
)

qt_add_executable(codicier src/main.cpp src/OfflineGuard.cpp)
target_compile_options(codicier PRIVATE -Wall -Wextra)
target_link_libraries(codicier PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick codicier_uiplugin)

enable_testing()
add_subdirectory(tests)
add_subdirectory(spike)
