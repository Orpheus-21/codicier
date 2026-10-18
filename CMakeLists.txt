cmake_minimum_required(VERSION 3.21)

project(codicier VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# clang-tidy and editors read build/compile_commands.json.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(codicier src/main.cpp src/OfflineGuard.cpp)

qt_add_qml_module(codicier
    URI Codicier
    QML_FILES qml/Main.qml
)

target_compile_options(codicier PRIVATE -Wall -Wextra)
target_link_libraries(codicier PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick)

enable_testing()
add_subdirectory(tests)
