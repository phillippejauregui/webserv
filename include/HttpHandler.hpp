/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpHandler.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/29 18:17:37 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/27 09:36:45 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPHANDLER_HPP
#define HTTPHANDLER_HPP

# include <string>
# include <iostream>
# include <sstream>
# include <sys/stat.h>
# include <unistd.h>
# include <fcntl.h>
# include "HttpRequest.hpp"
# include "LocationConfig.hpp"
# include "HttpResponse.hpp"
# include "ServerConfig.hpp"
# include "CGI.hpp"

struct MultipartPart {
    std::string filename;
    std::string content;
    std::string contentType;
};

HttpRequest		parseRequest(std::string const &buffer);
HttpResponse	serveFile(const std::string &fullPath);
HttpResponse	execute(HttpRequest const &req, LocationConfig const &loc, ServerConfig const &server);
LocationConfig	route(HttpRequest const &req, ServerConfig const &config);
HttpResponse	handleGet(HttpRequest const &req, LocationConfig const &location, std::string path, ServerConfig const &server);
HttpResponse	handlePost(HttpRequest const &request, LocationConfig const &location);
HttpResponse    handleDelete(const std::string &path);
std::string		resolvePath(const HttpRequest &req, ServerConfig const &server, const LocationConfig &loc);
bool            parseRequestFromBuffer(const std::string &buf, HttpRequest &outReq, size_t &consumed);
#endif
