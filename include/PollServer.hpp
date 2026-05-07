/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PollServer.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/14 16:50:23 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/06 19:55:08 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef POLLSERVER_HPP
#define POLLSERVER_HPP

# include "ClientState.hpp"
# include "ClientConnection.hpp"
# include "ServerSocket.hpp"
# include "HttpRequest.hpp"
# include "HttpResponse.hpp"
# include "HttpHandler.hpp"
# include <poll.h>
# include <vector>
# include <map>

class PollServer{
	private :
		std::vector<pollfd>					_fds;
		std::vector<ServerSocket*>			_servers;
		std::map<int, ClientConnection*>	_clients;
		std::map<int, ClientState>			_states;
		std::map<int, size_t>				_clientServerIndex;

		PollServer(const PollServer &src);
		PollServer&							operator=(const PollServer &rhs);
		void								_addFd(int fd);
		void								_removeFd(int fd);
		void								_newConnection(int serverFd);
		void								_clientEvent(size_t index, const std::vector<ServerConfig> &servers);
		void								_enableWrite(int fd);
		void								_disableWrite(int fd);

	public :
		PollServer();
		~PollServer();

		void	addServer(ServerConfig const &server);
		void	runServer(const std::vector<ServerConfig> &servers);
};

#endif

