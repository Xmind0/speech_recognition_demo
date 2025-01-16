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

        // 按钮行
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 20

            // 录音按钮
            Rectangle {
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

            // 测试按钮
            Rectangle {
                width: 80
                height: 80
                radius: width / 2
                color: "#2196F3"

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        recognizer.testPcmFile()
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "测试PCM"
                    color: "white"
                    font.bold: true
                }
            }
            Rectangle {
                width: 80
                height: 80
                radius: width / 2
                color: "green"

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        recognizer.testMicrophone()
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "测麦"
                    color: "white"
                    font.bold: true
                }
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
