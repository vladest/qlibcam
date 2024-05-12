import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import CamerasManager

ApplicationWindow {
    width: 800
    height: 600
    visible: true
    property LibCamera currentCamera: null
    title: qsTr("Camera: ") + (currentCamera !== null ? currentCamera.model : "None")
    property int cameraIndex: -1

    onCurrentCameraChanged: {
        if (currentCamera !== null) {
            currentCamera.videoSink = videoOutput.videoSink
            comboBoxFormats.model = currentCamera.formats
        }
    }

    function resetRects() {
        for (var i = 0; i < videoOutput.data.length; ++i) {
            var item = videoOutput.data[i];
            if (item !== null && item.objectName === "tf2FilterBox") {
                item.destroy();
            }
        }
    }

    Component {
        id: tf2FilterBox
        Item {
            objectName: "tf2FilterBox"
            property alias text: txt.text
            property alias conf: conftxt.text
            property color boxColor: "red"
            Row {
                spacing: 5
                Text {
                    id: txt
                    anchors.top: parent.top
                    font.pixelSize: 16
                    color: boxColor
                }
                Text {
                    id: conftxt
                    anchors.top: parent.top
                    font.pixelSize: 16
                    color: boxColor
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 20
                color: "transparent"
                border.width: 2
                border.color: boxColor
            }
        }
    }

    Connections {
        target: tf2Filter
        function onProcessingFinished(results) {
            var r = results.rects();
            var names = results.names();
            var confidences = results.confidences();
            var colors = results.colors();
            resetRects();
            for (var i = 0; i < r.length; i++) {
                var xr = videoOutput.width / videoOutput.sourceRect.width;
                var yr = videoOutput.height / videoOutput.sourceRect.height;
                var rect = tf2FilterBox.createObject(videoOutput);
                rect.text = names[i]
                rect.conf = confidences[i]
                rect.boxColor = colors[i]

                rect.x = /*video.x + */r[i].x * xr;
                rect.y = /*video.y + */r[i].y * yr;
                rect.width = r[i].width * xr;
                rect.height = r[i].height * yr;
            }
        }
    }

    header: ToolBar {
        Row {
            Button {
                text: "Start"
                height: 50
                enabled: currentCamera !== null && currentCamera.isCapturing === false
                onClicked: {
                    console.warn("Start")
                    if (currentCamera !== null) {
                        CamerasManager.startCapture(currentCamera.id, CamerasManager.ViewFinderRole)
                    }
                }
            }
            Button {
                text: "Stop"
                height: 50
                enabled: currentCamera !== null && currentCamera.isCapturing === true
                onClicked: {
                    console.warn("Stop")
                    if (currentCamera !== null) {
                        CamerasManager.stopCapture(currentCamera.id)
                    }
                }
            }
            ComboBox {
                id: comboBox
                width: 300
                height: 50
                enabled: currentCamera !== null && currentCamera.isCapturing === false
                model: CamerasManager.camerasModels
                onCurrentIndexChanged: {
                    console.warn("Camera selected:", comboBox.currentText, currentIndex)
                    currentCamera = CamerasManager.cameras[currentIndex]
                }
            }

            ComboBox {
                id: comboBoxFormats
                enabled: currentCamera !== null && currentCamera.isCapturing === false
                width: 100
                height: 50
                currentIndex: -1
                onCountChanged: {
                    if (count > 0)
                        currentIndex = 0
                }
                onCurrentValueChanged: {
                    console.warn("Format selected:", comboBoxFormats.currentValue)
                    if (currentIndex >= 0) {
                        comboBoxResolutions.model = null
                        comboBoxResolutions.model = currentCamera.availableResolutionsStrings(comboBoxFormats.currentValue)
                        // set the highest resolution
                        // when model changes, the currentIndex is reset to 0 as soon as count becomes > 0
                        // so we need to set it after model is set
                        comboBoxResolutions.currentIndex = comboBoxResolutions.count - 1
                        currentCamera.setPrefferedFormat(comboBoxFormats.currentValue)
                    }
                }
            }

            ComboBox {
                id: comboBoxResolutions
                enabled: currentCamera !== null && currentCamera.isCapturing === false
                width: 100
                height: 50
                currentIndex: -1
                onCurrentValueChanged: {
                    console.warn("Resolution selected:", comboBoxResolutions.currentValue)
                    currentCamera.setPrefferedResolution(comboBoxResolutions.currentValue)
                }
            }

        }
    }
    VideoOutput {
        id: videoOutput
        anchors.fill: parent
    }

    footer: RowLayout {
        TextField {
            id: textField
            Layout.fillWidth: true
            text: barcodeFilter.captured
        }
    }
}
