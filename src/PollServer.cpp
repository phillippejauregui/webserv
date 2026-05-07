/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PollServer.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/14 17:49:36 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/06 19:54:48 by cjauregu         ###   ########.fr       */
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
			_clientServerIndex[clientStatus] = i;
		}
	}
}

void	PollServer::_clientEvent(size_t index, const std::vector<ServerConfig> &servers){
	int			clientFd;
	std::string	buffer;
	HttpRequest	request;

	clientFd = _fds[index].fd;
	if (_clients[clientFd]->handleRead() == false){
		delete _clients[clientFd];
		_clients.erase(clientFd);
		_states.erase(clientFd);
		_removeFd(clientFd);
		return;
	}
	buffer = _clients[clientFd]->getReadBuffer();
	// request	 = parseRequest(buffer);
	// HARDCODE : WILL REMOVE
	size_t serverIndex = _clientServerIndex[clientFd];
	_clients[clientFd]->prepResponse(servers[serverIndex]);
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

	_servers.push_back(new ServerSocket(server));
	fd = _servers.back()->getFd();
	_addFd(fd);
}

void	PollServer::runServer(const std::vector<ServerConfig> &servers){
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
					_clientEvent(i, servers);
				if (_fds[i].revents & POLLOUT){
					clientFd = _fds[i].fd;
					_clients[clientFd]->handleWrite();
					if (_clients[clientFd]->getOffset() >= _clients[clientFd]->getBuffer().size())
						_disableWrite(_clients[clientFd]->getFd());
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
