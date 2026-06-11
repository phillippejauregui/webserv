#ifndef CLIENTCONNECTION_HPP
#define CLIENTCONNECTION_HPP

# include <string>
# include <map>
# include <unistd.h>
# include <sys/socket.h>
# include <fstream>
# include <sstream>
# include <deque>
# include <cstdlib>
# include <stdint.h>
# include <cerrno>
# include <cstring>
# include "ServerConfig.hpp"
# include "HttpResponse.hpp"

class ClientConnection {
private :
    int             _fd;
    std::string     _readBuffer;
    bool            _headersParsed;
    size_t          _expectedLength;
    size_t          _headerEnd;

    std::deque<std::string> _responseQueue;
    size_t      _writeOffset;
    uint64_t    _requestId;

    ClientConnection(ClientConnection const &src);
    ClientConnection& operator=(ClientConnection const &rhs);

public :
    ClientConnection(int fd);
    ~ClientConnection();

    std::string const& getReadBuffer() const;
    void replaceReadBuffer(const std::string& s);
    void popReadBytes(size_t n);

    void enqueueResponse(const std::string &resp);
    bool hasPendingResponses() const;
    const std::string &currentResponse() const;
    void popResponse();

    bool handleWrite();
    bool writeComplete() const;
    void clearWrite();
    void prepResponse(const HttpResponse &response);
    bool handleRead();
    bool isRequestComplete() const;
    void resetReadState();
    void reset();
    void setRequestId(uint64_t id);
    uint64_t getRequestId() const;
    std::string getBuffer() const;
    size_t getOffset() const;
    int getFd() const;
};

#endif
