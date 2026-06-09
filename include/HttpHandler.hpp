/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpHandler.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/29 18:17:37 by lylrandr          #+#    #+#             */
/*   Updated: 2026/06/08 18:59:17 by cjauregu         ###   ########.fr       */
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



HttpRequest     parseRequest(std::string const &buffer, size_t bodyOffset, size_t bodyLength);
HttpResponse	serveFile(const std::string &fullPath);
HttpResponse    buildError(int code, std::string const &message, ServerConfig const &config);
HttpResponse	execute(HttpRequest const &req, LocationConfig const &loc, ServerConfig const &server);
LocationConfig	route(HttpRequest const &req, ServerConfig const &config);
HttpResponse	handleGet(LocationConfig const &location, std::string path);
HttpResponse    handlePost(const HttpRequest& request, const LocationConfig& location, const ServerConfig& server, const std::string& path);
HttpResponse    handleDelete(const std::string& path, const ServerConfig& server);
std::string		resolvePath(const HttpRequest &req, const LocationConfig &loc);
bool            parseRequestFromBuffer(const std::string &buf, HttpRequest &outReq, size_t &consumed);

#endif
