/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PollServer.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/14 17:49:36 by lylrandr          #+#    #+#             */
/*   Updated: 2026/06/08 18:56:29 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "PollServer.hpp"
 
PollServer::PollServer() {}
 
PollServer::~PollServer() {
    for (std::map<int, ClientConnection*>::iterator it = _clients.begin();
         it != _clients.end(); it++)
        delete it->second;
    for (size_t i = 0; i < _servers.size(); i++)
        delete _servers[i];
    for (std::map<int, CGIProcess>::iterator it = _cgiProcesses.begin();
         it != _cgiProcesses.end(); ++it) {
        CGIProcess &cgi = it->second;
        if (cgi.pid != -1)
            kill(cgi.pid, SIGKILL);
        if (cgi.inFd != -1)  close(cgi.inFd);
        if (cgi.outFd != -1) close(cgi.outFd);
    }
}

void PollServer::_addFd(int fd, short events) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    _fds.push_back(pfd);
}
 
void PollServer::_removeFd(int fd) {
    for (size_t i = 0; i < _fds.size(); i++) {
        if (_fds[i].fd == fd) {
            _fds.erase(_fds.begin() + i);
            return;
        }
    }
}
 
void PollServer::_enableWrite(int fd) {
    for (size_t i = 0; i < _fds.size(); i++) {
        if (_fds[i].fd == fd) {
            _fds[i].events |= POLLOUT;
            return;
        }
    }
}
 
void PollServer::_disableWrite(int fd) {
    for (size_t i = 0; i < _fds.size(); i++) {
        if (_fds[i].fd == fd) {
            _fds[i].events &= ~POLLOUT;
            return;
        }
    }
}

void PollServer::_newConnection(int serverFd) {
    for (size_t i = 0; i < _servers.size(); i++) {
        if (_servers[i]->getFd() == serverFd) {
            int clientFd = _servers[i]->acceptClient();
            if (clientFd < 0)
                return;
            setNonBlocking(clientFd);
            _addFd(clientFd);
            _clients[clientFd] = new ClientConnection(clientFd);
            _states[clientFd] = ClientState();
            _clientConfig[clientFd] = _configs[i];
            return;
        }
    }
}

void PollServer::registerCGI(int clientFd, CGIProcess &cgi) {
    _cgiProcesses[clientFd] = cgi;
    _pipeToClient[cgi.outFd] = clientFd;
    _addFd(cgi.outFd, POLLIN);
 
    if (!cgi.inputDone) {
        _pipeToClient[cgi.inFd] = clientFd;
        _addFd(cgi.inFd, POLLOUT);
    } else {
        close(cgi.inFd);
        _cgiProcesses[clientFd].inFd = -1;
    }
}
 
void PollServer::_handleCGIWrite(int pipeFd)
{
    std::map<int,int>::iterator itClient = _pipeToClient.find(pipeFd);
    if (itClient == _pipeToClient.end())
        return;
    int clientFd = itClient->second;
    CGIProcess &cgi = _cgiProcesses[clientFd];
    if (cgi.inputDone) {
        _removeFd(pipeFd);
        return;
    }
    const std::string &buf = cgi.inputBuffer;
    size_t remaining = buf.size() - cgi.inputOffset;
    if (remaining == 0) {
        close(cgi.inFd);
        cgi.inputDone = true;
        _removeFd(pipeFd);
        return;
    }
    ssize_t n = write(pipeFd, buf.data() + cgi.inputOffset, remaining);
    if (n > 0) {
        cgi.inputOffset += n;
        if (cgi.inputOffset >= buf.size()) {
            close(cgi.inFd);
            cgi.inputDone = true;
            _removeFd(pipeFd);
        }
        return;
    }
    close(cgi.inFd);
    cgi.inputDone = true;
    _removeFd(pipeFd);
}

bool PollServer::decodeChunkedBody(ClientConnection* client, ClientState& state) {
    const std::string& buf = client->getReadBuffer();

    size_t headerEnd = buf.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return false;

    size_t pos = headerEnd + 4;
    std::string decodedBody;

    while (true) {
        size_t lineEnd = buf.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            return false;
        std::string hex = buf.substr(pos, lineEnd - pos);
        size_t chunkSize = strtol(hex.c_str(), NULL, 16);
        //std::cerr << "[CHUNK] size hex=" << hex
                  //<< " dec=" << chunkSize << "\n";
        pos = lineEnd + 2;
        if (chunkSize == 0) {
            if (buf.size() < pos + 2)
                return false;
           // std::cerr << "[CHUNK] final chunk received\n";
            std::string newBuf = buf.substr(0, headerEnd + 4);
            newBuf += decodedBody;
            client->replaceReadBuffer(newBuf);
            state.requestReady = true;
            state.bodyBytesRead = decodedBody.size();
            return true;
        }
        if (buf.size() < pos + chunkSize + 2)
            return false;
        decodedBody.append(buf.substr(pos, chunkSize));
        //std::cerr << "[CHUNK] appended " << chunkSize
                  //<< " bytes (total=" << decodedBody.size() << ")\n";
        pos += chunkSize;
        if (buf.substr(pos, 2) != "\r\n")
            return false;

        pos += 2;
    }
}

void PollServer::_handleCGIRead(int pipeFd)
{
    std::map<int,int>::iterator itClient = _pipeToClient.find(pipeFd);
    if (itClient == _pipeToClient.end())
        return;
    int clientFd = itClient->second;
    CGIProcess &cgi = _cgiProcesses[clientFd];
    char buf[4096];
    ssize_t n = read(pipeFd, buf, sizeof(buf));
    if (n > 0) {
        cgi.outputBuffer.append(buf, n);
        return;
    }
    _removeFd(pipeFd);
    close(pipeFd);
    cgi.outputDone = true;
    int status;
    waitpid(cgi.pid, &status, WNOHANG);
    std::string outputBuffer = cgi.outputBuffer;
    _pipeToClient.erase(pipeFd);
    if (cgi.inFd != -1)
        _pipeToClient.erase(cgi.inFd);
    _cgiProcesses.erase(clientFd);
    if (_clients.find(clientFd) == _clients.end())
        return;
    HttpResponse res = parseCGIOutput(outputBuffer);
    ClientConnection *client = _clients[clientFd];
    client->prepResponse(res);
    client->handleWrite();
    _enableWrite(clientFd);
}
 
void PollServer::_finishCGI(int clientFd) {
    std::map<int, CGIProcess>::iterator it = _cgiProcesses.find(clientFd);
    if (it == _cgiProcesses.end())
        return;
    pid_t pid  = it->second.pid;
    int   inFd = it->second.inFd;
    std::string output = it->second.outputBuffer;
    int status;
    waitpid(pid, &status, WNOHANG);
    if (inFd != -1) {
        _removeFd(inFd);
        _pipeToClient.erase(inFd);
        close(inFd);
    }
    _cgiProcesses.erase(clientFd);
    if (_clients.find(clientFd) == _clients.end())
        return;
    HttpResponse response = parseCGIOutput(output);
    _clients[clientFd]->prepResponse(response);
    _enableWrite(clientFd);
}
 
void PollServer::_abortCGI(int clientFd) {
    std::map<int, CGIProcess>::iterator it = _cgiProcesses.find(clientFd);
    if (it == _cgiProcesses.end())
        return;
    pid_t pid   = it->second.pid;
    int   inFd  = it->second.inFd;
    int   outFd = it->second.outFd;
    if (pid != -1) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, WNOHANG);
    }
    if (inFd != -1) {
        _removeFd(inFd);
        _pipeToClient.erase(inFd);
        close(inFd);
    }
    if (outFd != -1) {
        _removeFd(outFd);
        _pipeToClient.erase(outFd);
        close(outFd);
    }
    _cgiProcesses.erase(clientFd);
    if (_clients.find(clientFd) == _clients.end())
        return;
    ClientConnection *client = _clients[clientFd];
    client->prepResponse(buildError(504, "Gateway Timeout", _clientConfig[clientFd]));
    client->handleWrite();
    _enableWrite(clientFd);
}

void PollServer::_clientEvent(size_t index) {
    int clientFd = _fds[index].fd;
    if (_clients.find(clientFd) == _clients.end())
        return;
    ClientConnection *client = _clients[clientFd];
    ClientState &state = _states[clientFd];
    if (!client->handleRead()) {
        _abortCGI(clientFd);
        delete client;
        _clients.erase(clientFd);
        _states.erase(clientFd);
        _clientConfig.erase(clientFd);
        _removeFd(clientFd);
		close(clientFd);
        return;
    }
    while (true) {
        const std::string &buf = client->getReadBuffer();
        if (buf.empty())
            break;
        if (!state.headersComplete) {
            size_t headerEnd = buf.find("\r\n\r\n");
            if (headerEnd == std::string::npos) {
                break;
            }
            state.headersComplete = true;
            std::string headerBlock = buf.substr(0, headerEnd + 4);
            if (headerBlock.find("Transfer-Encoding: chunked") != std::string::npos) {
                state.isChunked = true;
                state.contentLength = 0;
                //std::cerr << "[CHUNK] Chunked mode enabled\n";
            }
            else {
                size_t pos = headerBlock.find("Content-Length:");
                if (pos != std::string::npos) {
                    size_t start = pos + 15;
                    size_t end = headerBlock.find("\r\n", start);
                    state.contentLength = std::atoi(headerBlock.substr(start, end - start).c_str());
                } else {
                    state.contentLength = 0;
                }
            }
            size_t maxBody = _clientConfig[clientFd].client_max_body_size;
            if (maxBody > 0 && state.contentLength > maxBody) {
                //std::cerr << "[HEADERS] Body too large, returning 413\n";
                client->prepResponse(buildError(413, "Request Entity Too Large", _clientConfig[clientFd]));
                client->handleWrite();
                _enableWrite(clientFd);
                state = ClientState();
                break;
            }            
        }
        if (!state.requestReady) {
            if (state.isChunked)
            {
                if (!decodeChunkedBody(client, state))
                    break;
            }
            else {
                size_t headerEnd = buf.find("\r\n\r\n");
                size_t bodyStart = headerEnd + 4;
                size_t bodySize = buf.size() - bodyStart;
                if (bodySize < state.contentLength) {
                    break;
                }
                state.requestReady = true;
            }
        }

        if (!state.requestReady)
            break;

        //std::cerr << "===== [DBG RAW REQUEST fd=" << clientFd << "] =====\n";
        //std::cerr << client->getReadBuffer() << "\n";
        //std::cerr << "===== [END RAW REQUEST] =====\n";
        HttpRequest request;
        size_t consumed = 0;
        if (!parseRequestFromBuffer(buf, request, consumed)) {
            break;
        }
        client->popReadBytes(consumed);
        LocationConfig loc = route(request, _clientConfig[clientFd]);
        if (!loc.redirect.empty()) {
            HttpResponse redir;
            redir.statusCode = 301;
            redir.statusMessage = "Moved Permanently";
            redir.headers["Location"] = loc.redirect;
            redir.headers["Content-Length"] = "0";
            client->prepResponse(redir);
            client->handleWrite();
            _enableWrite(clientFd);
            state = ClientState();
            continue;
        }
        std::string path = resolvePath(request, loc);
        if (isCGIvalid(loc, path) &&
            (request.method == "GET" || request.method == "POST")) {
            try {
                CGIProcess cgi = launchCGI(request, _clientConfig[clientFd], loc, path);
                registerCGI(clientFd, cgi);
            } catch (std::exception &e) {
                //std::cerr << "[CGI] Launch failed: " << e.what() << "\n";
                client->prepResponse(buildError(500, "Internal Server Error", _clientConfig[clientFd]));
                client->handleWrite();
                _enableWrite(clientFd);
            }
            state = ClientState();
            continue;
        }
        HttpResponse response = execute(request, loc, _clientConfig[clientFd]);
        client->prepResponse(response);
        client->handleWrite();
        _enableWrite(clientFd);
        state = ClientState();
        continue;        
    }
}

void PollServer::addServer(ServerConfig const &server) {
    _configs.push_back(server);
    _servers.push_back(new ServerSocket(server));
    _addFd(_servers.back()->getFd());
}

void PollServer::runServer() {
    while (1) {
        time_t now = time(NULL);
        for (std::map<int, CGIProcess>::iterator it = _cgiProcesses.begin();
             it != _cgiProcesses.end(); ) {
            if (now - it->second.startTime > 10) {
                int clientFd = it->first;
                std::cerr << "[CGI] Timeout for client fd=" << clientFd << "\n";
                ++it;
                _abortCGI(clientFd);
            } else {
                ++it;
            }
        }
        int ret = poll(&_fds[0], _fds.size(), 1000);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("poll() failed");
        }
        for (size_t i = 0; i < _fds.size(); ++i) {
            int   fd      = _fds[i].fd;
            short revents = _fds[i].revents;
 
            if (revents == 0)
                continue;
            _fds[i].revents = 0;
            if (_pipeToClient.find(fd) != _pipeToClient.end()) {
                int clientFd = _pipeToClient[fd];
                if (_cgiProcesses.find(clientFd) == _cgiProcesses.end())
                    continue;
                CGIProcess &cgi = _cgiProcesses[clientFd];
 
                if (fd == cgi.inFd && (revents & POLLOUT)) {
                    _handleCGIWrite(fd);
                } else if (fd == cgi.outFd && (revents & POLLIN)) {
                    _handleCGIRead(fd);
                } else if (revents & (POLLERR | POLLHUP)) {
                    if (fd == cgi.outFd)
                        _handleCGIRead(fd);
                }
                i = (size_t)-1;
                continue;
            }
            bool isServer = false;
            for (size_t j = 0; j < _servers.size(); ++j) {
                if (_servers[j]->getFd() == fd) {
                    isServer = true;
                    break;
                }
            }
            if (isServer) {
                if (revents & POLLIN)
                    _newConnection(fd);
                i = (size_t)-1;
                continue;
            }
            if (_clients.find(fd) == _clients.end())
                continue;
            if (revents & POLLIN) {
                for (size_t k = 0; k < _fds.size(); ++k) {
                    if (_fds[k].fd == fd) {
                        _clientEvent(k);
                        break;
                    }
                }
                if (_clients.find(fd) == _clients.end()) {
                    i = (size_t)-1;
                    continue;
                }
            }
            if (revents & POLLOUT) {
                if (_clients.find(fd) == _clients.end()) {
                    i = (size_t)-1;
                    continue;
                }
                ClientConnection *client = _clients[fd];
                if (!client->handleWrite()) {
                    _abortCGI(fd);
                    delete client;
                    _clients.erase(fd);
                    _states.erase(fd);
                    _clientConfig.erase(fd);
                    _removeFd(fd);
                    close(fd);
                    i = (size_t)-1;
                    continue;
                }
                if (client->writeComplete()) {
                    client->popResponse();
                    if (client->hasPendingResponses()) {
                        _enableWrite(fd);
                    } else {
                        _disableWrite(fd);
                        if (client->getReadBuffer().empty()) {
                            client->resetReadState();
                            client->clearWrite();
                        }
                    }
                }
            }
            if (revents & (POLLERR | POLLHUP)) {
                if (_clients.find(fd) != _clients.end()) {
                    _abortCGI(fd);
                    delete _clients[fd];
                    _clients.erase(fd);
                    _states.erase(fd);
                    _clientConfig.erase(fd);
                    _removeFd(fd);
                    close(fd);
                    i = (size_t)-1;
                }
            }
        }
    }
}
