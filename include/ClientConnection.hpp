/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientConnection.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/13 18:27:14 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/06 19:53:46 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENTCONNECTION_HPP
#define CLIENTCONNECTION_HPP

# include <string>
# include <map>
# include <unistd.h>
# include <sys/socket.h>
#include <fstream>
#include <sstream>
#include "ServerConfig.hpp"

class	ClientConnection{
	private :
		int			_fd;
		std::string	_readBuffer;
		std::string	_writeBuffer;
		size_t		_writeOffset;

		ClientConnection(ClientConnection const &src);
		ClientConnection&	operator=(ClientConnection const &rhs);
	public :
		ClientConnection(int fd);
		~ClientConnection();

		std::string				getBuffer() const;
		size_t					getOffset() const;
		int						getFd() const;
		std::string const&		getReadBuffer() const;
		bool					handleRead();
		bool					handleWrite();
		void					prepResponse(const ServerConfig &conf);
};

#endif
