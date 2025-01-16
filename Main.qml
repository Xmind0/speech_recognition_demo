// main.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SpeechRecognition 1.0

ApplicationWindow {
    visible: true
    width: 600
    height: 400
    title: "语音识别Demo"

    SpeechRecognizer {
        id: recognizer
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // 录音按钮
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 80
            height: 80
            radius: width / 2
            color: recognizer.recording ? "red" : "green"

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (!recognizer.recording) {
                        recognizer.startRecording()
                    } else {
                        recognizer.stopRecording()
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                text: recognizer.recording ? "停止" : "录音"
                color: "white"
                font.bold: true
            }
        }

        // 识别结果显示区域
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TextArea {
                text: recognizer.text
                readOnly: true
                wrapMode: TextArea.Wrap
                background: Rectangle {
                    color: "white"
                    border.color: "gray"
                    radius: 5
                }
            }
        }
    }
}
