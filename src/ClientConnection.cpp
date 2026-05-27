#include "ClientConnection.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

ClientConnection::ClientConnection(int fd)
: _fd(fd), _headersParsed(false), _expectedLength(0), _headerEnd(0),
  _writeOffset(0), _requestId(0)
{
}

ClientConnection::~ClientConnection(){
    close(_fd);
}

/* Read buffer access */
std::string const& ClientConnection::getReadBuffer() const {
    return _readBuffer;
}

void ClientConnection::popReadBytes(size_t n) {
    if (n >= _readBuffer.size()) {
        _readBuffer.clear();
    } else {
        _readBuffer.erase(0, n);
    }
}

/* Write queue API */
void ClientConnection::enqueueResponse(const std::string &resp) {
    bool wasEmpty = _responseQueue.empty();
    _responseQueue.push_back(resp);
    if (wasEmpty)
        _writeOffset = 0;
}

bool ClientConnection::hasPendingResponses() const {
    return !_responseQueue.empty();
}

const std::string &ClientConnection::currentResponse() const {
    return _responseQueue.front();
}

void ClientConnection::popResponse() {
    if (!_responseQueue.empty())
        _responseQueue.pop_front();
    _writeOffset = 0;
}

/* Write helpers */
bool ClientConnection::handleWrite() {
    if (_responseQueue.empty()) {
        return true;
    }
    const std::string &buf = _responseQueue.front();
    const char *data = buf.c_str() + _writeOffset;
    size_t rest = buf.size() - _writeOffset;
    ssize_t sent = send(_fd, data, rest, 0);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
        return false;
    }
    _writeOffset += static_cast<size_t>(sent);
    return true;
}

bool ClientConnection::writeComplete() const {
    if (_responseQueue.empty()) return true;
    return _writeOffset >= _responseQueue.front().size();
}

void ClientConnection::clearWrite() {
    _responseQueue.clear();
    _writeOffset = 0;
}

/* Response prep: enqueue serialized response */
void ClientConnection::prepResponse(const HttpResponse &response)
{
    std::ostringstream out;
    out << "HTTP/1.1 " << response.statusCode << " " << response.statusMessage << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = response.headers.begin();
            it != response.headers.end(); it++)
        out << it->first << ": " << it->second << "\r\n";
    out << "\r\n";
    out << response.body;
    enqueueResponse(out.str());
}

/* Read handling */
bool ClientConnection::handleRead() {
    char buf[4096];
    ssize_t n = recv(_fd, buf, sizeof(buf), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
        return false;
    }
    if (n == 0)
        return false;
    _readBuffer.append(buf, static_cast<size_t>(n));
    if (!_headersParsed) {
        size_t pos = _readBuffer.find("\r\n\r\n");
        if (pos != std::string::npos) {
            _headersParsed = true;
            _headerEnd = pos + 4;
            std::string headerPart = _readBuffer.substr(0, pos + 4);
            size_t clPos = headerPart.find("Content-Length:");
            if (clPos != std::string::npos) {
                clPos += 15;
                while (clPos < headerPart.size() && headerPart[clPos] == ' ')
                    ++clPos;
                _expectedLength = std::atoi(headerPart.c_str() + clPos);
            } else {
                _expectedLength = 0;
            }
        }
    }
    return true;
}

bool ClientConnection::isRequestComplete() const {
    if (!_headersParsed)
        return false;
    if (_expectedLength == 0)
        return true;
    size_t bodySize = _readBuffer.size() - _headerEnd;
    return bodySize >= _expectedLength;
}

void ClientConnection::resetReadState() {
    _readBuffer.clear();
    _headersParsed = false;
    _expectedLength = 0;
    _headerEnd = 0;
}

void ClientConnection::reset() {
    resetReadState();
    clearWrite();
    _requestId = 0;
}

void ClientConnection::setRequestId(uint64_t id) {
    _requestId = id;
}
uint64_t ClientConnection::getRequestId() const {
    return _requestId;
}

std::string ClientConnection::getBuffer() const {
    if (_responseQueue.empty()) return std::string();
    return _responseQueue.front();
}

size_t ClientConnection::getOffset() const {
    return _writeOffset;
}

int ClientConnection::getFd() const {
    return _fd;
}
