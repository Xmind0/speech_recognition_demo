# Qt 语音识别演示程序 🎙️

<div align="center">

[![Qt](https://img.shields.io/badge/Qt-6.5.2+-41CD52?style=flat-square&logo=qt&logoColor=white)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/平台-Windows%20|%20Linux%20|%20macOS-blue?style=flat-square)](https://www.qt.io/download)

[![主页](https://github.com/Xmind0/speech_recognition_demo/blob/master/Screenshot%20from%202025-01-16%2023-21-22.png)](LICENSE)

一个基于 Qt 6 和科大讯飞语音识别 API 的实时语音识别演示程序

</div>

---

## 📌 项目简介

这是一个使用 Qt 开发的语音识别演示程序，能够实时录制音频并转换为文字。项目使用科大讯飞开放平台的语音识别服务，支持中文普通话的实时识别。

## ✨ 功能特点

- 实时音频录制和识别
- 基于 WebSocket 的数据传输
- 支持中文普通话识别
- 实时显示识别结果
- 音频数据实时分析
- 完整的错误处理机制

## 🔧 环境要求

### 基本要求
- Qt 6.5.2 或更高版本
- 科大讯飞开放平台账号
- 支持的操作系统：
  - Windows 10/11
  - Linux
  - macOS

### 开发环境
- Qt Creator
- 支持 C++17 的编译器
- 稳定的网络连接

## 🚀 快速开始

### 配置步骤

1. 注册科大讯飞开放平台账号
   - 访问 [科大讯飞开放平台](https://www.xfyun.cn/)
   - 完成账号注册和实名认证

2. 创建应用并获取密钥
   ```
   APPID：应用标识
   API Key：接口密钥
   API Secret：接口密钥密码
   ```

3. 在main.cpp代码中配置密钥信息

app_id
API_KEY
API_SECRET

### 编译运行

1. 克隆项目代码：
   ```bash
   git clone https://github.com/yourusername/qt-speech-recognition.git
   cd qt-speech-recognition
   ```

2. 使用 Qt Creator 打开项目：
   ```bash
   qtcreator CMakeLists.txt
   ```

3. 配置项目并编译
4. 运行程序

## 📖 使用说明

### 基本操作

1. 启动程序
2. 点击录音按钮开始录制
3. 对着麦克风说话
4. 识别结果会实时显示在界面上
5. 再次点击按钮停止录音

### 音频参数设置

| 参数 | 值 |
|------|-----|
| 采样率 | 16kHz |
| 声道数 | 单声道 |
| 采样格式 | 16位 PCM |
| 帧大小 | 12800 字节（40ms音频） |

## 🔍 技术实现

### 核心功能
- 使用 `QAudioSource` 进行音频采集
- 使用 `QWebSocket` 进行实时数据传输
- 实现音频数据缓冲和帧管理
- 支持音频有效性检测
- 包含完整的错误处理机制

## ⚠️ 注意事项

1. 使用前检查
   - 确保麦克风正常工作
   - 检查网络连接是否稳定
   - 验证 API 密钥配置是否正确

2. 使用建议
   - 建议在安静的环境中使用
   - 说话语速保持正常
   - 注意 API 调用限制

## 🐛 常见问题

### 问题排查指南

1. **认证错误**
   - 检查 API 密钥配置
   - 确认网络连接状态
   - 验证系统时间是否准确

2. **无声音输入**
   - 检查麦克风权限
   - 验证系统音频设置
   - 测试麦克风是否正常

3. **识别不准确**
   - 确保环境够安静
   - 说话清晰、语速适中
   - 检查音频输入质量

## 📝 开源协议

本项目采用 MIT 开源协议 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 📚 参考资源

### 相关文档
- [科大讯飞开放平台文档](https://www.xfyun.cn/doc/asr/voicedictation/API.html)
- [Qt 官方文档](https://doc.qt.io/)

### 技术支持
- API 相关问题：[科大讯飞支持中心](https://www.xfyun.cn/support)
- Qt 相关问题：[Qt 论坛](https://forum.qt.io/)

---

<div align="center">

使用 Qt 和科大讯飞 API 用 ❤️ 制作

[报告问题](https://github.com/yourusername/qt-speech-recognition/issues) · [功能建议](https://github.com/yourusername/qt-speech-recognition/issues)

</div>
