/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PollServer.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/14 16:50:23 by lylrandr          #+#    #+#             */
/*   Updated: 2026/06/01 16:40:39 by cjauregu         ###   ########.fr       */
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
# include "CGI.hpp"
# include <poll.h>
# include <vector>
# include <map>
# include <ctime>
#include "CGIProcess.hpp"
# include <signal.h>

class PollServer {
    private:
        std::vector<pollfd>                 _fds;
        std::vector<ServerSocket*>          _servers;
        std::map<int, ClientConnection*>    _clients;
        std::map<int, ClientState>          _states;
        std::vector<ServerConfig>           _configs;
        std::map<int, ServerConfig>         _clientConfig;

        // CGI tracking
        std::map<int, CGIProcess>           _cgiProcesses;  // clientFd -> CGIProcess
        std::map<int, int>                  _pipeToClient;  // pipeFd   -> clientFd

        PollServer(const PollServer &src);
        PollServer& operator=(const PollServer &rhs);

        void    _addFd(int fd, short events = POLLIN);
        void    _removeFd(int fd);
        void    _newConnection(int serverFd);
        void    _clientEvent(size_t index);
        void    _enableWrite(int fd);
        void    _disableWrite(int fd);

        // CGI pipe handlers
        void    _handleCGIWrite(int pipeFd);
        void    _handleCGIRead(int pipeFd);
        void    _finishCGI(int clientFd);
        void    _abortCGI(int clientFd);

    public:
        PollServer();
        ~PollServer();

        void    addServer(ServerConfig const &server);
        void    runServer();
        void    registerCGI(int clientFd, CGIProcess &cgi);
};

#endif