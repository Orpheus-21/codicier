import QtQuick
import QtTest

TestCase {
    name: "Smoke"

    function test_mainWindowSize() {
        const component = Qt.createComponent("Codicier", "Main")
        compare(component.status, Component.Ready, component.errorString())

        const window = component.createObject(null, { visible: false }) as Window
        verify(window)
        verify(window.width >= 800)
        verify(window.height >= 600)
        window.destroy()
    }
}
