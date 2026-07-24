// cam_stream.cpp — ESP32-CAM MJPEG 流客户端实现

#include "cam_stream.h"
#include "config.h"

CamStream::CamStream()
    : _port(80)
    , _state(ParseState::WAITING_HEADER)
    , _contentLength(0)
    , _bytesRead(0)
    , _frameBuffer(nullptr)
    , _frameBufferSize(FRAME_BUFFER_SIZE)
    , _frameBufferLen(0)
    , _lastCharCR(false)
    , _lastFrameTime(0)
    , _connectTime(0)
    , _frameCount(0)
    , _connected(false)
    , _httpStatusCode(0)
    , _headersComplete(false)
{
    _frameBuffer = (uint8_t*)ps_malloc(_frameBufferSize);
    if (!_frameBuffer) {
        _frameBuffer = (uint8_t*)malloc(_frameBufferSize);
    }
}

CamStream::~CamStream() {
    stop();
    if (_frameBuffer) {
        free(_frameBuffer);
        _frameBuffer = nullptr;
    }
}

void CamStream::setFrameCallback(FrameCallback cb) {
    _frameCallback = cb;
}

bool CamStream::begin(const String& url) {
    _url = url;
    stop();

    if (!parseUrl()) {
        Serial.printf("[CamStream] URL 解析失败: %s\n", url.c_str());
        return false;
    }

    return sendHttpRequest();
}

void CamStream::stop() {
    _client.stop();
    _connected = false;
    _state = ParseState::WAITING_HEADER;
    _contentLength = 0;
    _bytesRead = 0;
    _frameBufferLen = 0;
    _lineBuffer = "";
    _lastCharCR = false;
    _httpStatusCode = 0;
    _headersComplete = false;
}

bool CamStream::isConnected() const {
    return _connected && _client.connected();
}

void CamStream::loop() {
    if (!_client.connected()) {
        if (_connected) {
            Serial.println("[CamStream] 流已断开");
            _connected = false;
        }
        // 自动重连
        if (millis() - _connectTime > CAM_RECONNECT_MS) {
            _connectTime = millis();
            Serial.println("[CamStream] 尝试重连...");
            begin(_url);
        }
        return;
    }

    while (_client.available()) {
        char c = _client.read();

        if (_state == ParseState::READING_DATA) {
            // 数据模式：直接读字节
            processDataByte((uint8_t)c);
        } else {
            // 行模式：拼接行直到遇到 \n
            if (c == '\r') {
                _lastCharCR = true;
            } else if (c == '\n') {
                if (_lastCharCR) {
                    // 去掉末尾的 \r
                    if (!_lineBuffer.isEmpty()) {
                        _lineBuffer.remove(_lineBuffer.length() - 1);
                    }
                }
                processLine(_lineBuffer);
                _lineBuffer = "";
                _lastCharCR = false;
            } else {
                _lineBuffer += c;
                _lastCharCR = false;
            }
        }
    }
}

bool CamStream::parseUrl() {
    // 解析 "http://host:port/path"
    if (!_url.startsWith("http://")) {
        return false;
    }

    String rest = _url.substring(7);  // 去掉 "http://"

    // 分离 host 和 path
    int pathStart = rest.indexOf('/');
    if (pathStart < 0) {
        _host = rest;
        _path = "/";
    } else {
        _host = rest.substring(0, pathStart);
        _path = rest.substring(pathStart);
    }

    // 分离 host 和 port
    int colonPos = _host.indexOf(':');
    if (colonPos >= 0) {
        _port = _host.substring(colonPos + 1).toInt();
        _host = _host.substring(0, colonPos);
    }

    return true;
}

bool CamStream::sendHttpRequest() {
    if (_client.connected()) {
        _client.stop();
    }

    if (!_client.connect(_host.c_str(), _port, 5000)) {
        Serial.printf("[CamStream] 连接 %s:%d 失败\n", _host.c_str(), _port);
        return false;
    }

    _client.printf("GET %s HTTP/1.1\r\n", _path.c_str());
    _client.printf("Host: %s:%d\r\n", _host.c_str(), _port);
    _client.printf("Connection: keep-alive\r\n");
    _client.printf("Accept: */*\r\n");
    _client.printf("\r\n");

    resetParseState();
    _connected = true;
    Serial.printf("[CamStream] 已连接 %s:%d%s\n", _host.c_str(), _port, _path.c_str());
    return true;
}

void CamStream::resetParseState() {
    _state = ParseState::WAITING_HEADER;
    _contentLength = 0;
    _bytesRead = 0;
    _frameBufferLen = 0;
    _lineBuffer = "";
    _lastCharCR = false;
    _httpStatusCode = 0;
    _headersComplete = false;
}

void CamStream::processLine(const String& line) {
    switch (_state) {
        case ParseState::WAITING_HEADER: {
            // 解析 "HTTP/1.1 200 OK"
            if (line.startsWith("HTTP/")) {
                int space1 = line.indexOf(' ');
                if (space1 >= 0) {
                    _httpStatusCode = line.substring(space1 + 1).toInt();
                    if (_httpStatusCode != 200) {
                        Serial.printf("[CamStream] HTTP %d\n", _httpStatusCode);
                    }
                }
            }

            // 空行 = 头部结束
            if (line.isEmpty()) {
                _headersComplete = true;
                _state = ParseState::WAITING_BOUNDARY;
                Serial.println("[CamStream] HTTP 头部接收完毕，等待视频流...");
            }
            break;
        }

        case ParseState::WAITING_BOUNDARY: {
            // 行以 "--" 开头说明是边界
            if (line.startsWith("--")) {
                _boundary = line;
                _state = ParseState::READING_HEADERS;
                // 此时解析出来的 boundary 后面可能有帧头部信息
                Serial.printf("[CamStream] 边界: %s\n", _boundary.c_str());
            }
            break;
        }

        case ParseState::READING_HEADERS: {
            if (line.startsWith("Content-Length:")) {
                _contentLength = line.substring(15).toInt();
            } else if (line.startsWith("Content-Type:")) {
                // 确认是 image/jpeg
            }

            // 空行 = 帧头部结束，开始读数据
            if (line.isEmpty() && _contentLength > 0) {
                // 确保缓冲足够大
                if (_contentLength > _frameBufferSize) {
                    Serial.printf("[CamStream] 帧过大: %zu bytes (缓冲 %zu)\n",
                                  _contentLength, _frameBufferSize);
                    // 跳过此帧
                    _state = ParseState::READING_DATA;
                    _bytesRead = 0;
                    _frameBufferLen = 0;
                } else {
                    _state = ParseState::READING_DATA;
                    _bytesRead = 0;
                    _frameBufferLen = 0;
                }
            }
            break;
        }

        default:
            break;
    }
}

void CamStream::processDataByte(uint8_t byte) {
    if (_bytesRead < _contentLength && _bytesRead < _frameBufferSize) {
        _frameBuffer[_bytesRead++] = byte;
    }

    if (_bytesRead >= _contentLength) {
        // 一帧接收完毕
        _frameBufferLen = _bytesRead;
        onFrameComplete();
        _state = ParseState::WAITING_BOUNDARY;
    }
}

void CamStream::onFrameComplete() {
    _frameCount++;
    _lastFrameTime = millis();

    // 调用回调
    if (_frameCallback && _frameBufferLen > 0) {
        _frameCallback(_frameBuffer, _frameBufferLen);
    }
}

String CamStream::getStatus() const {
    char buf[96];
    if (_connected) {
        snprintf(buf, sizeof(buf), "CAM: %s:%d | frames: %lu | last: %lums ago",
                 _host.c_str(), _port, _frameCount,
                 _lastFrameTime > 0 ? millis() - _lastFrameTime : 0);
    } else {
        snprintf(buf, sizeof(buf), "CAM: disconnected");
    }
    return String(buf);
}

