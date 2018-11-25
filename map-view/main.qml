import QtQuick 2.11
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4
import QtQuick.Dialogs 1.3
import QtQuick.Layouts 1.11
import QtQuick.Window 2.11
import QtPositioning 5.12
import QtLocation 5.12
import Qt.labs.qmlmodels 1.0
import Qt.GeoJson 1.0

ApplicationWindow {
    visible: true
    width: 1024
    height: 1024
    menuBar: mainMenu
    title: qsTr("GeoJSON Viewer")

    Shortcut {
        sequence: "Ctrl+P"
        onActivated: {
            geoJsoner.dumpGeoJSON(geoJsoner.toGeoJson(miv));
        }
    }

    FileDialog {
        visible: false
        id: fileDialog
        title: "Please choose a GeoJSON file"
        folder: shortcuts.home
        onAccepted: {
            console.log("You chose: " + fileDialog.fileUrls)
            geoJsoner.load(fileDialog.fileUrl)
//            Qt.quit()
        }
        onRejected: {
            console.log("Canceled")
//            Qt.quit()
        }
        //Component.onCompleted: visible = true
    }

    MenuBar {
        id: mainMenu

        Menu {
            title: "GeoJSON"
            id : geoJsonMenu
            MenuItem {
                text: "Open..."
                id : geoJsonMenuOpen
                /*action: openGeoJsonFile*/

                onTriggered: {
                    fileDialog.open()
                }
            }
        }
    }

    GeoJsoner {
        id: geoJsoner
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: Plugin { name: "osm" }
        zoomLevel: 3

        MapItemView {
            id: miv
            model: geoJsoner.model
            delegate: GeoJsonDelegate {

            }
        }

//        MapItemView {
//            model: [ {
//                "latitude" : 52.0,
//                "longitude" : 0,
//                "color" : "blue"
//            } ]

//            delegate: MapCircle {
//                center: QtPositioning.coordinate(modelData.latitude, modelData.longitude)
//                radius: 100*1000
//                color: modelData.color
//            }
//        }
    }
}
