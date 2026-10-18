# Static mock screens for design review, and the tool that renders them.
qt_add_executable(mockshot mockshot.cpp)
target_compile_options(mockshot PRIVATE -Wall -Wextra)
target_link_libraries(mockshot PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick codicier_uiplugin)
