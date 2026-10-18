cmake_minimum_required(VERSION 3.21)

project(codicier VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# clang-tidy and editors read build/compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Qml Quick)
# toml++ reads the project files (docs/format.md).
find_package(tomlplusplus 3 REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)

# The project model: plain C++ and Qt Core, with no UI.
add_library(codicier_core STATIC src/model/ProjectStore.cpp src/model/TomlWriter.cpp)
target_include_directories(codicier_core PUBLIC src/model)
target_compile_options(codicier_core PRIVATE -Wall -Wextra)
target_link_libraries(codicier_core PUBLIC Qt6::Core)
# Arch builds toml++ as a shared library. Use only its headers, in header-only
# mode, so the app needs no toml++ library at run time.
get_target_property(TOML_INCLUDE tomlplusplus::tomlplusplus INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(codicier_core SYSTEM PUBLIC ${TOML_INCLUDE})
target_compile_definitions(codicier_core PUBLIC TOML_HEADER_ONLY=1)

# The QML module is a static library, so the app and the tests link the same module.
# Its files go to build/qml/Codicier, so qmllint and tools find them by import path.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
qt_add_library(codicier_ui STATIC)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(codicier_ui
    URI Codicier
    QML_FILES qml/Main.qml qml/Theme.qml qml/Icon.qml qml/IconPaths.js
        qml/FocusRing.qml qml/Button.qml qml/IconButton.qml qml/TextField.qml
        qml/ListItem.qml qml/Pane.qml qml/Splitter.qml qml/Menu.qml
    RESOURCES fonts/Literata.ttf fonts/Literata-Italic.ttf fonts/Inter.ttf
)

qt_add_executable(codicier src/main.cpp src/OfflineGuard.cpp)
target_compile_options(codicier PRIVATE -Wall -Wextra)
target_link_libraries(codicier PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick codicier_uiplugin)

enable_testing()
add_subdirectory(tests)
add_subdirectory(spike)
add_subdirectory(mocks)
