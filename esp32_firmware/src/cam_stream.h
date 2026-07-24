// cam_stream.h — ESP32-CAM MJPEG 视频流客户端
//
// 通过 WiFi 连接 ESP32-CAM 的 HTTP MJPEG 流，
// 解析 multipart/x-mixed-replace 边界，提取完整的 JPEG 帧。
//
// 用法:
//   CamStream cam;
//   cam.setFrameCallback([](const uint8_t* jpeg, size_t len) {
//       // 处理每一帧 JPEG 数据
//   });
//   cam.begin("http://192.168.1.100:80/stream");

#ifndef CAM_STREAM_H_
#define CAM_STREAM_H_

#include <Arduino.h>
#include <WiFi.h>
#include <functional>

// 帧回调类型：收到完整 JPEG 帧时触发
using FrameCallback = std::function<void(const uint8_t* jpegData, size_t jpegLen)>;

class CamStream {
public:
    CamStream();
    ~CamStream();

    // 开始连接视频流
    // url 格式: "http://192.168.1.100:80/stream"
    bool begin(const String& url);

    // 断开连接
    void stop();

    // 是否已连接并正在接收流
    bool isConnected() const;

    // 必须在主 loop 中定期调用，驱动数据接收和解析
    void loop();

    // 设置帧回调
    void setFrameCallback(FrameCallback cb);

    // 获取当前状态描述
    String getStatus() const;

private:
    WiFiClient _client;
    String _url;
    String _host;
    uint16_t _port;
    String _path;

    FrameCallback _frameCallback;

    // MJPEG 解析状态机
    enum class ParseState {
        WAITING_HEADER,       // 等待 HTTP 响应头
        WAITING_BOUNDARY,     // 等待 --boundary
        READING_HEADERS,      // 读取 Content-Type, Content-Length
        READING_DATA,         // 读取 JPEG 数据
    };

    ParseState _state;
    String _boundary;         // multipart 分隔符
    size_t _contentLength;    // 当前帧的 Content-Length
    size_t _bytesRead;        // 已读字节数

    // 帧数据缓冲（动态分配）
    uint8_t* _frameBuffer;
    size_t _frameBufferSize;
    size_t _frameBufferLen;

    // 行缓冲
    String _lineBuffer;
    bool _lastCharCR;

    // 状态统计
    unsigned long _lastFrameTime;
    unsigned long _connectTime;
    unsigned long _frameCount;
    bool _connected;

    // 内部方法
    bool parseUrl();
    bool sendHttpRequest();
    void resetParseState();
    void processLine(const String& line);
    void processDataByte(uint8_t byte);
    void onFrameComplete();

    // HTTP 响应解析
    int _httpStatusCode;
    bool _headersComplete;
};

#endif  // CAM_STREAM_H_
