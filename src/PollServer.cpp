/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PollServer.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/14 17:49:36 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/27 16:46:56 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "PollServer.hpp"
#include "Parser.hpp"

PollServer::PollServer(){
}

PollServer::~PollServer(){
	for (std::map<int, ClientConnection*>::iterator it = _clients.begin(); it != _clients.end(); it++)
		delete it->second;
	for (size_t i = 0; i < _servers.size(); i++)
		delete _servers[i];
}

void	PollServer::_addFd(int fd){
	struct pollfd	pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_fds.push_back(pfd);
}

void	PollServer::_removeFd(int fd){
	for(size_t i = 0; i < _fds.size(); i++){
		if (_fds[i].fd == fd){
			close(_fds[i].fd);
			_fds.erase(_fds.begin() + i);
		}
	}
}

void	PollServer::_newConnection(int serverFd){
	int	clientStatus;

	clientStatus = 0;
	for(size_t i = 0; i < _servers.size(); i++){
		if (_servers[i]->getFd() == serverFd){
			clientStatus =  _servers[i]->acceptClient();
			if (clientStatus < 0)
				return;
			_addFd(clientStatus);
			_clients[clientStatus] = new ClientConnection(clientStatus);
			_states[clientStatus] = ClientState();
			_clientConfig[clientStatus] = _configs[i];
		}
	}
}

void PollServer::_clientEvent(size_t index)
{
    int clientFd = _fds[index].fd;
    ClientConnection* client = _clients[clientFd];
    ClientState& state = _states[clientFd];
    if (!client->handleRead()) {
        delete client;
        _clients.erase(clientFd);
        _states.erase(clientFd);
        _removeFd(clientFd);
        return;
    }
    while (true) {
        const std::string& buf = client->getReadBuffer();
        if (buf.empty())
            break;
        if (!state.headersComplete) {
            size_t headerEnd = buf.find("\r\n\r\n");
            if (headerEnd == std::string::npos)
                break;
            state.headersComplete = true;
            size_t pos = buf.find("Content-Length:");
            if (pos != std::string::npos) {
                size_t start = pos + 15;
                size_t end = buf.find("\r\n", start);
                std::string lenStr = buf.substr(start, end - start);
                state.contentLength = std::atoi(lenStr.c_str());
            } else {
                state.contentLength = 0;
            }
        }
        if (!state.requestReady) {
            size_t headerEnd = buf.find("\r\n\r\n");
            size_t bodyStart = headerEnd + 4;
            size_t bodySize = buf.size() - bodyStart;
            if (bodySize < state.contentLength)
                break;
            state.requestReady = true;
        }
        HttpRequest request;
        size_t consumed = 0;
        if (!parseRequestFromBuffer(buf, request, consumed))
            break;
        client->popReadBytes(consumed);
        LocationConfig loc = route(request, _clientConfig[clientFd]);
        HttpResponse response = execute(request, loc, _clientConfig[clientFd]);
        client->prepResponse(response);
        state = ClientState();
    }
    if (client->hasPendingResponses())
        _enableWrite(clientFd);
}



void	PollServer::_enableWrite(int fd){
	for(size_t i = 0; i < _fds.size(); i++){
		if(_fds[i].fd == fd){
			_fds[i].events |= POLLOUT;
		}
	}
}

void	PollServer::_disableWrite(int fd){
	for (size_t i = 0; i < _fds.size(); i++){
		if (_fds[i].fd == fd){
			_fds[i].events &= ~POLLOUT;
		}
	}
}

void	PollServer::addServer(ServerConfig const &server){
	int	fd;

	_configs.push_back(server);
	_servers.push_back(new ServerSocket(server));
	fd = _servers.back()->getFd();
	_addFd(fd);
}

void	PollServer::runServer(){
	bool	isServer;
	int		ret;
	int		clientFd;

	while(1){
		ret = poll(_fds.data(), _fds.size(), -1);
		if (ret < 0)
			throw std::runtime_error("poll() failed");
		for(size_t i = 0; i < _fds.size(); i++){
			isServer = false;
			for(size_t j = 0; j < _servers.size(); j++){
				if (_servers[j]->getFd() == _fds[i].fd)
					isServer = true;
			}
			if (isServer){
				if (_fds[i].revents & POLLIN)
					_newConnection(_fds[i].fd);
			}
			else{
				if (_fds[i].revents & POLLIN)
					_clientEvent(i);
					if (_fds[i].revents & POLLOUT){
						clientFd = _fds[i].fd;
						ClientConnection *client = _clients[clientFd];
					
						if (!client->handleWrite()) {
							delete client;
							_clients.erase(clientFd);
							_states.erase(clientFd);
							_removeFd(clientFd);
							break;
						}
						if (client->writeComplete()) {
							client->popResponse();
							if (client->hasPendingResponses()) {
								_enableWrite(clientFd);
							} else {
								_disableWrite(clientFd);
								if (_clients[clientFd]->getReadBuffer().empty()) {
									_clients[clientFd]->resetReadState();
									_clients[clientFd]->clearWrite();
								}
							}
						}
					}					
				if (_fds[i].revents & (POLLERR | POLLHUP)){
					clientFd = _fds[i].fd;
					delete _clients[clientFd];
					_clients.erase(clientFd);
					_states.erase(clientFd);
					_removeFd(clientFd);
					break;
				}
			}
		}
	}
}
