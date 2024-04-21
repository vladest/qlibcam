import QtQuick
import QtQuick.Controls
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
                enabled: currentCamera.isCapturing === false
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
}
