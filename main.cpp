// main.cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWebSocket>
#include <QAudioSource>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QTimer>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QFile>
#include <QProcess>  // 用于调用外部程序
#include <QDir>


namespace {
constexpr int FRAME_SIZE = 12800;  // 每帧音频大小 (16k采样率 * 40ms * 2字节)
}

class SpeechRecognizer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)

public:
    explicit SpeechRecognizer(QObject *parent = nullptr)
        : QObject(parent)
        , m_recording(false)
        , m_sessionValid(false)
        , m_waitingForConnection(false)
    {
        // 打印所有可用的音频输入设备
        qDebug() << "Available audio input devices:";
        for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
            qDebug() << " - " << device.description();
        }

        QAudioFormat format;
        format.setSampleRate(16000);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Int16);

        QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
        if (!inputDevice.isFormatSupported(format)) {
            qWarning() << "Default format not supported, trying to use nearest format";
            format = inputDevice.preferredFormat();
        }

        qDebug() << "Using audio format:"
                 << "\nSample rate:" << format.sampleRate()
                 << "\nChannels:" << format.channelCount()
                 << "\nSample format:" << format.sampleFormat()
                 << "\nBytes per frame:" << format.bytesPerFrame();

        m_buffer = new QBuffer(this);
        m_buffer->open(QIODevice::ReadWrite);

        m_audioSource = new QAudioSource(inputDevice, format, this);
        m_audioSource->setBufferSize(32768);  // 增大缓冲区大小

        // 监控音频状态
        connect(m_audioSource, &QAudioSource::stateChanged,
                this, [this](QAudio::State state) {
                    qDebug() << "Audio state changed:" << state;
                    if (state == QAudio::StoppedState && m_audioSource->error() != QAudio::NoError) {
                        qDebug() << "Audio error:" << m_audioSource->error();
                    }
                });

        // WebSocket 连接和错误处理
        connect(&m_webSocket, &QWebSocket::connected, this, [this]() {
            qDebug() << "WebSocket connected successfully";
            m_waitingForConnection = false;
            if (m_recording) {
                QTimer::singleShot(100, this, &SpeechRecognizer::sendFirstFrame); // 延迟100毫秒
            }
        });

        connect(&m_webSocket, &QWebSocket::textMessageReceived,
                this, &SpeechRecognizer::onTextMessage);

        connect(&m_webSocket, &QWebSocket::errorOccurred,
                this, [this](QAbstractSocket::SocketError error) {
                    qDebug() << "WebSocket error:" << error << m_webSocket.errorString();
                });

        connect(&m_webSocket, &QWebSocket::stateChanged,
                this, [this](QAbstractSocket::SocketState state) {
                    qDebug() << "WebSocket state changed:" << state;
                });
    }

    QString text() const { return m_text; }
    bool recording() const { return m_recording; }

public slots:
    void startRecording() {
        if (m_recording) return;

        qDebug() << "Starting recording...";

        // 添加音频设备检查
        QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
        if (!inputDevice.isNull()) {
            qDebug() << "Using audio device:" << inputDevice.description();
        } else {
            qDebug() << "No audio input device found!";
            return;
        }

        // 重置所有状态
        m_text.clear();
        emit textChanged();
        m_buffer->buffer().clear();
        m_buffer->seek(0);
        m_sessionValid = false;
        m_firstFrame = true;
        m_waitingForConnection = true;
        m_frameStatus = STATUS_FIRST_FRAME;
        m_bufferReady = false;
        m_processedPosition = 0;  // 添加处理位置跟踪

        // 设置录音状态
        m_recording = true;
        emit recordingChanged();

        // 启动音频采集
        m_audioSource->start(m_buffer);

        // 启动定时器监控音频数据
        m_timer.disconnect();
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            if (!m_recording) return;

            // 检查缓冲区大小
            int availableData = m_buffer->size() - m_processedPosition;
            
            if (!m_bufferReady && availableData >= MIN_BUFFER_SIZE) {
                m_bufferReady = true;
                qDebug() << "Buffer ready with size:" << availableData;
                
                // 缓冲区准备好后，开始WebSocket连接
                QString url = QString("wss://iat-api.xfyun.cn/v2/iat?authorization=%1&date=%2&host=%3")
                                  .arg(generateAuthorization())
                                  .arg(QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy HH:mm:ss") + " GMT")
                                  .arg("iat-api.xfyun.cn");

                qDebug() << "Connecting to WebSocket URL:" << url;
                m_webSocket.open(QUrl(url));
                return;
            }

            // 只有当缓冲区准备好且WebSocket连接成功后才处理音频数据
            if (m_bufferReady && m_sessionValid) {
                if (m_firstFrame) {
                    sendFirstFrame();
                } else if (availableData >= FRAME_SIZE) {
                    sendAudioData();
                }
            }
        });
        
        m_timer.setInterval(40);  // 40ms间隔
        m_timer.start();
    }

    void stopRecording() {
        if (!m_recording) {
            return;  // 防止重复调用
        }

        qDebug() << "Stopping recording...";
        m_recording = false;
        emit recordingChanged();

        m_timer.stop();
        disconnect(&m_timer, &QTimer::timeout, this, &SpeechRecognizer::sendAudioData);

        // 确保发送结束帧
        if (m_webSocket.state() == QAbstractSocket::ConnectedState) {
            QJsonObject json;
            QJsonObject audioData;
            audioData["status"] = 2;
            audioData["format"] = "audio/L16;rate=16000";
            audioData["encoding"] = "raw";
            audioData["audio"] = "";
            json["data"] = audioData;

            QString message = QJsonDocument(json).toJson();
            m_webSocket.sendTextMessage(message);
            qDebug() << "Sent final end frame";

            // 使用一个延迟关闭的槽函数
            QTimer::singleShot(1000, this, [this]() {
                qDebug() << "Closing WebSocket connection after delay";
                m_webSocket.close();
            });
        }
        m_audioSource->stop();
    }

    void testPcmFile() {
        if (m_recording) {
            qDebug() << "Already recording, ignoring request";
            return;
        }

        QString filePath = "/home/hcy/QT_pro/speech_recognition/iat_pcm_16k.pcm";
        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Failed to open PCM file at:" << filePath;
            qDebug() << "Error:" << file.errorString();
            return;
        }

        QByteArray fileData = file.readAll(); // 读取文件数据
        file.close();

        if (fileData.isEmpty()) {
            qDebug() << "Failed to read PCM file data";
            return;
        }

        // 详细打印PCM文件信息
        // qDebug() << "PCM file details:"
        //          << "\nTotal size:" << fileData.size() << "bytes"
        //          << "\nSample count:" << fileData.size() / 2
        //          << "\nDuration:" << (fileData.size() / 2.0 / 16000.0) << "seconds";

        // 检查PCM数据的有效性
        const int16_t* samples = reinterpret_cast<const int16_t*>(fileData.constData()); // 将文件数据转换为int16_t类型的数组
        int sampleCount = fileData.size() / 2; // 计算样本数量  2 byte 一个样本
        int16_t maxValue = 0; // 初始化最大值
        int16_t minValue = 0; // 初始化最小值
        bool hasNonZero = false; // 初始化是否有非零样本的标志

        for (int i = 0; i < sampleCount && i < 1000; ++i) { 
            if (samples[i] != 0) {
                hasNonZero = true;
                maxValue = std::max(maxValue, samples[i]);
                minValue = std::min(minValue, samples[i]);
            }
        }


        // qDebug() << "PCM data analysis:"
        //          << "\nFirst 32 bytes:" << fileData.left(32).toHex()
        //          << "\nHas non-zero samples:" << hasNonZero
        //          << "\nMax value in first 1000 samples:" << maxValue
        //          << "\nMin value in first 1000 samples:" << minValue;

        if (!hasNonZero) {
            qDebug() << "Warning: PCM file appears to contain only zeros!";
            return;
        }

        // 重置状态
        m_text.clear();
        emit textChanged();

        // 准备缓冲区
        m_buffer->buffer().clear();
        m_buffer->seek(0); // 将缓冲区位置设置为0
        m_buffer->buffer() = fileData;
        

        // 设置状态
        m_sessionValid = false; // 会话有效
        m_firstFrame = true; // 是否是第一帧
        m_waitingForConnection = true; // 是否等待连接
        m_frameStatus = STATUS_FIRST_FRAME; // 帧状态
        m_recording = true; // 录音状态
        emit recordingChanged();

        // 启动定时器监控数据发送
        m_timer.disconnect();
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            if (!m_recording) return;

            // qDebug() << "Timer tick - Buffer status:"
            //          << "\nTotal size:" << m_buffer->size()
            //          << "\nPosition:" << m_buffer->pos()
            //          << "\nBytes available:" << m_buffer->bytesAvailable();

            if (m_sessionValid && !m_firstFrame) {
                sendAudioData();
            }
        });
        m_timer.setInterval(40);  // 40ms间隔，对应于每帧的时长
        m_timer.start();

        // 连接WebSocket
        QString url = QString("wss://iat-api.xfyun.cn/v2/iat?authorization=%1&date=%2&host=%3")
                          .arg(generateAuthorization())
                          .arg(QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy HH:mm:ss") + " GMT")
                          .arg("iat-api.xfyun.cn");

        qDebug() << "Connecting to WebSocket URL:" << url;
        m_webSocket.open(QUrl(url));
    }

    void testMicrophone() {
        if (m_recording) {
            qDebug() << "Already recording, ignoring request";
            return;
        }

        // 检查音频设备
        QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
        if (inputDevice.isNull()) {
            qDebug() << "Error: No audio input device found!";
            return;
        }
        qDebug() << "Using audio device:" << inputDevice.description();

        // 清理之前的缓冲区
        m_buffer->buffer().clear();
        m_buffer->seek(0);

        // 设置测试状态
        m_recording = true;
        emit recordingChanged();

        // 开始录音
        qDebug() << "Starting microphone test...";
        m_audioSource->start(m_buffer);

        // 创建音频分析定时器
        QTimer *analysisTimer = new QTimer(this);
        connect(analysisTimer, &QTimer::timeout, this, [this]() {
            if (m_buffer->size() > 0) {
                // 获取最新的音频数据
                QByteArray audioData = m_buffer->buffer();
                const int16_t* samples = reinterpret_cast<const int16_t*>(audioData.constData());
                int sampleCount = audioData.size() / 2;

                // 计算音量级别
                double rms = 0.0;
                int16_t maxAmp = 0;
                for (int i = 0; i < sampleCount; ++i) {
                    rms += samples[i] * samples[i];
                    maxAmp = qMax(maxAmp, qAbs(samples[i]));
                }
                rms = sqrt(rms / sampleCount);

                // 计算分贝值 (参考值：16位音频的最大值是32768)
                double db = 20 * log10(rms / 32768.0);

                // 输出音频分析结果
                qDebug() << "Audio Analysis:"
                         << "\nBuffer size:" << audioData.size() << "bytes"
                         << "\nPeak amplitude:" << maxAmp
                         << "\nRMS:" << rms
                         << "\nDB level:" << db << "dB";

                // 简单的音量等级显示
                QString volumeBar = "|";
                int barLength = qMax(0, qMin(50, int((db + 60) * 1.25))); // 将-60dB到-20dB映射到0-50的范围
                volumeBar += QString(barLength, '#') + QString(50 - barLength, '-') + "|";
                qDebug() << "Volume level:" << volumeBar;

                // 检测是否有声音
                if (db > -50) { // -50dB作为有效声音的阈值
                    qDebug() << "Sound detected!";
                }
            }
        });
        analysisTimer->start(100); // 每100ms分析一次

        // 5秒后停止测试
        QTimer::singleShot(5000, this, [this, analysisTimer]() {
            // 停止录音
            m_audioSource->stop();
            analysisTimer->stop();
            analysisTimer->deleteLater();

            // 保存测试音频
            QString testFilePath = "/home/hcy/QT_pro/speech_recognition/test.pcm";
            QFile testFile(testFilePath);
            if (testFile.open(QIODevice::WriteOnly)) {
                testFile.write(m_buffer->buffer());
                testFile.close();
                qDebug() << "Test recording saved to:" << testFilePath;

                // 分析完整录音
                QByteArray audioData = m_buffer->buffer();
                qDebug() << "Test recording summary:"
                         << "\nTotal duration: 5 seconds"
                         << "\nTotal samples:" << audioData.size() / 2
                         << "\nFile size:" << audioData.size() << "bytes";
            }

            // 重置状态
            m_recording = false;
            emit recordingChanged();
            m_buffer->buffer().clear();
            m_buffer->seek(0);

            qDebug() << "Microphone test completed";
            qDebug() << "测试文件保存路径：" << testFilePath;
        });
    }

signals:
    void textChanged();
    void recordingChanged();

private slots:
    void onConnected() {
        qDebug() << "WebSocket connected successfully";
        m_waitingForConnection = false;
        m_sessionValid = true;  // 设置会话有效

        // 连接成功后等待一小段时间再发送第一帧，确保有足够的音频数据
        QTimer::singleShot(100, this, [this]() {
            if (m_recording) {
                sendFirstFrame();
            }
        });
    }

    void onTextMessage(const QString &message) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);

        if (error.error != QJsonParseError::NoError) {
            qDebug() << "JSON parse error:" << error.errorString();
            return;
        }

        QJsonObject obj = doc.object();
        qDebug() << "Received WebSocket message:" << message;

        int code = obj["code"].toInt();
        if (code != 0) {
            qDebug() << "Error response, code:" << code
                     << "message:" << obj["message"].toString();
            stopRecording();
            return;
        }

        QJsonObject data = obj["data"].toObject();
        QJsonObject result = data["result"].toObject();

        // 解析识别结果
        if (result.contains("ws")) {
            QJsonArray words = result["ws"].toArray();
            QString text;
            for (const QJsonValue &word : words) {
                QJsonArray cw = word.toObject()["cw"].toArray();
                for (const QJsonValue &item : cw) {
                    text += item.toObject()["w"].toString();
                }
            }

            if (!text.isEmpty()) {
                m_text += text;
                qDebug() << "Recognized text:" << text;
                emit textChanged();
            }
        }

        // 检查是否是最后一帧的响应
        int status = data["status"].toInt();
        if (status == 2) {
            qDebug() << "Received final response";
            stopRecording();
        }
    }

    void sendAudioData() {
        if (!m_recording || !m_sessionValid) {
            return;
        }

        int availableData = m_buffer->size() - m_processedPosition;
        
        if (availableData < FRAME_SIZE) {
            if (availableData > 0) {
                m_buffer->seek(m_processedPosition);
                QByteArray remainingData = m_buffer->read(availableData);
                m_processedPosition += availableData;
                
                QJsonObject json;
                QJsonObject data;
                data["status"] = STATUS_LAST_FRAME;
                data["format"] = "audio/L16;rate=16000";
                data["encoding"] = "raw";
                data["audio"] = QString(remainingData.toBase64());
                json["data"] = data;

                QString message = QJsonDocument(json).toJson();
                qDebug() << "Sending last frame with size:" << remainingData.size();
                m_webSocket.sendTextMessage(message);
            }
            stopRecording();
            return;
        }

        m_buffer->seek(m_processedPosition);
        QByteArray frameData = m_buffer->read(FRAME_SIZE);
        m_processedPosition += FRAME_SIZE;

        // 验证数据有效性
        const int16_t* samples = reinterpret_cast<const int16_t*>(frameData.constData());
        int sampleCount = frameData.size() / 2;
        bool hasValidData = false;
        
        for (int i = 0; i < sampleCount; ++i) {
            if (samples[i] != 0) {
                hasValidData = true;
                break;
            }
        }

        if (!hasValidData) {
            return;
        }

        QJsonObject json;
        QJsonObject data;
        data["status"] = STATUS_CONTINUE_FRAME;
        data["format"] = "audio/L16;rate=16000";
        data["encoding"] = "raw";
        data["audio"] = QString(frameData.toBase64());
        json["data"] = data;

        QString message = QJsonDocument(json).toJson();
        m_webSocket.sendTextMessage(message);
    }

private:
    QString generateAuthorization() {
        const QString API_KEY = "";      // 需要修改自己的API_KEY
        const QString API_SECRET = "";  // 需要修改自己的

        // 生成 GMT 格式的时间
        QDateTime currentTime = QDateTime::currentDateTimeUtc();
        QString dateStr = currentTime.toString("ddd, dd MMM yyyy HH:mm:ss") + " GMT";

        // 生成HMAC-SHA256签名
        QString signStr = QString("host: iat-api.xfyun.cn\n"
                                  "date: %1\n"
                                  "GET /v2/iat HTTP/1.1")
                              .arg(dateStr);

        QByteArray signature = QMessageAuthenticationCode::hash(
                                   signStr.toUtf8(),
                                   API_SECRET.toUtf8(),
                                   QCryptographicHash::Sha256
                                   ).toBase64();

        QString authStr = QString(R"(api_key="%1", algorithm="hmac-sha256", )"
                                  R"(headers="host date request-line", signature="%2")")
                              .arg(API_KEY, signature);

        return authStr.toUtf8().toBase64();
    }

    void sendFirstFrame() {
        int availableData = m_buffer->size() - m_processedPosition;
        
        qDebug() << "Attempting to send first frame:"
                 << "\nBuffer size:" << m_buffer->size()
                 << "\nProcessed position:" << m_processedPosition
                 << "\nAvailable data:" << availableData;

        if (availableData < FRAME_SIZE) {
            qDebug() << "Not enough data for first frame, waiting...";
            return;
        }

        // 从当前处理位置读取数据
        m_buffer->seek(m_processedPosition);
        QByteArray firstFrameData = m_buffer->read(FRAME_SIZE);
        m_processedPosition += FRAME_SIZE;

        // 验证数据有效性
        const int16_t* samples = reinterpret_cast<const int16_t*>(firstFrameData.constData());
        int sampleCount = firstFrameData.size() / 2;
        bool hasValidData = false;
        
        for (int i = 0; i < sampleCount; ++i) {
            if (samples[i] != 0) {
                hasValidData = true;
                break;
            }
        }

        if (!hasValidData) {
            qDebug() << "First frame contains no valid audio data, waiting...";
            return;
        }

        QJsonObject json;

        // 公共参数
        QJsonObject common;
        common["app_id"] = "";  // 需要修改自己的app_id
        json["common"] = common;

        // 业务参数
        QJsonObject business;
        business["language"] = "zh_cn";
        business["domain"] = "iat";
        business["accent"] = "mandarin";
        business["vad_eos"] = 10000;
        business["dwa"] = "wpgs";
        business["pd"] = "game";
        business["ptt"] = 1;
        business["rlang"] = "zh-cn";
        business["vinfo"] = 1;
        business["nunum"] = 1;
        business["speex_size"] = 70;
        business["nbest"] = 1;
        business["wbest"] = 1;
        json["business"] = business;

        // 音频数据
        QJsonObject data;
        data["status"] = STATUS_FIRST_FRAME;
        data["format"] = "audio/L16;rate=16000";
        data["encoding"] = "raw";
        data["audio"] = QString(firstFrameData.toBase64());
        json["data"] = data;

        QString message = QJsonDocument(json).toJson();
        qDebug() << "Sending first frame with config:" << message;
        m_webSocket.sendTextMessage(message);

        m_frameStatus = STATUS_CONTINUE_FRAME;
        m_firstFrame = false;
        m_sessionValid = true;
    }

private:
    QWebSocket m_webSocket;
    QAudioSource *m_audioSource;
    QBuffer *m_buffer;
    QTimer m_timer;
    QString m_text;
    bool m_recording;
    bool m_firstFrame;
    bool m_sessionValid;
    bool m_waitingForConnection;  // 新增成员变量
    qint64 m_startTime;  // 添加开始时间记录

    enum FrameStatus {
        STATUS_FIRST_FRAME = 0,
        STATUS_CONTINUE_FRAME = 1,
        STATUS_LAST_FRAME = 2
    };

    FrameStatus m_frameStatus;

    static constexpr int MIN_BUFFER_SIZE = FRAME_SIZE * 2;  // 至少缓存两帧数据
    bool m_bufferReady = false;  // 标记缓冲区是否准备好
    qint64 m_processedPosition = 0;  // 跟踪已处理的数据位置
};


#include "main.moc"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    qmlRegisterType<SpeechRecognizer>("SpeechRecognition", 1, 0, "SpeechRecognizer");

    const QUrl url(u"qrc:/main.qml"_qs);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}
