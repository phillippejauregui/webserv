/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerSocket.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lylrandr <lylrandr@student.42lausanne.ch>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/09 14:56:17 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/04 11:05:36 by lylrandr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerSocket.hpp"

ServerSocket::ServerSocket(ServerConfig const &config){
	int	opt = 1;
	_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (_fd == -1)
		throw std::runtime_error("creation of socket failed");
	if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
		close(_fd);
		throw std::runtime_error("setsockopt failed");
	}
	_addr.sin_family = AF_INET;
	_addr.sin_port = htons(config.listen);
	_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(_fd, (struct sockaddr*)&_addr, sizeof(_addr)) == -1){
		close(_fd);
		throw std::runtime_error("bind failed");
	}
	if (listen(_fd, 128) == -1){
		close(_fd);
		throw std::runtime_error("listen failed");
	}
	if (setNonBlocking(_fd) < 0){
		throw std::runtime_error("socket blocked");
	}
}

ServerSocket::~ServerSocket(){
	close(_fd);
}

int	ServerSocket::getFd() const{
	return (this->_fd);
}

int	ServerSocket::acceptClient() const{
	return (accept(_fd, NULL, NULL));
}
