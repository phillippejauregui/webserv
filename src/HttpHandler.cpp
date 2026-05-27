/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpHandler.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/29 18:18:11 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/27 16:48:01 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpHandler.hpp"

HttpRequest	parseRequest(std::string const &buffer){
	std::string			body;
	std::string			key;
	std::string			value;
	std::string			line;
	HttpRequest			req;
	std::istringstream	stream(buffer);

	stream >> req.method >> req.uri >> req.version;
	std::getline(stream, line);
	while (std::getline(stream,line))
	{
		if (line == "\r" || line.empty())
			break;
		size_t pos = line.find(':');
		if (pos == std::string::npos)
			continue;
		key = line.substr(0, pos);
		value = line.substr(pos + 1);
		while (!value.empty() && value[0] == ' ')
			value.erase(0, 1);
		if (!value.empty() && value[value.size() - 1] == '\r')
			value.erase(value.size() - 1);
		req.headers[key] = value;
	}
	std::getline(stream, body, '\0');
	req.body = body;
	return (req);
}

LocationConfig route(HttpRequest const &req, ServerConfig const &config)
{
    const LocationConfig* best = NULL;

    for (size_t i = 0; i < config.locations.size(); i++) {
        const LocationConfig& loc = config.locations[i];
        if (req.uri.compare(0, loc.path.size(), loc.path) == 0) {
            if (!best || loc.path.size() > best->path.size()) {
                best = &loc;
            }
        }
    }
    if (!best) {
        std::cout << "Routing URI: " << req.uri
                  << " → matched location: / (default)" << std::endl;
        return config.locations[0];
    }
    std::cout << "Routing URI: " << req.uri
              << " → matched location: " << best->path << std::endl;
    return *best;
}

std::string resolvePath(const HttpRequest &req, const LocationConfig &loc)
{
    std::string uri = req.uri;
    size_t q = uri.find('?');
    if (q != std::string::npos)
        uri = uri.substr(0, q);
    if (uri.empty() || uri[0] != '/')
        uri = "/" + uri;
    std::string root = loc.root;
    if (!root.empty() && root.back() == '/')
        root.pop_back();
    std::string locPath = loc.path;
    if (locPath.empty())
        locPath = "/";
    if (locPath[0] != '/')
        locPath = "/" + locPath;
    if (locPath.size() > 1 && locPath.back() == '/')
        locPath.pop_back();
    std::string remainder;
    if (locPath == "/") {
        remainder = uri;
    } else {
        std::string prefix = locPath + "/";
        if (uri.compare(0, prefix.size(), prefix) == 0) {
            remainder = uri.substr(locPath.size());
        } else {
            remainder = uri;
        }
    }
    if (remainder.empty() || remainder[0] != '/')
        remainder = "/" + remainder;
    std::string path = root + remainder;
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        if (!path.empty() && path.back() != '/')
            path += "/";
        path += loc.index;
    }
    return path;
}

std::string getMimeType(const std::string& path)
{
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos)
		return "application/octet-stream";
	std::string ext = path.substr(dot + 1);
	std::cout << "Extension : " << ext << std::endl;

	if (ext == "html") return "text/html";
	if (ext == "css")  return "text/css";
	if (ext == "js")   return "application/javascript";
	if (ext == "png")  return "image/png";
	if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
	if (ext == "gif")  return "image/gif";
	if (ext == "ico")  return "image/x-icon";
	if (ext == "txt")  return "text/plain";

	return "application/octet-stream";
}

HttpResponse	serveFile(std::string const &path){
	HttpResponse	response;
	int				fd = open(path.c_str(), O_RDONLY);

	if (fd < 0)
	{
		response.statusCode = 404;
		response.statusMessage = "Not Found";
		response.body = "<html><body><h1>404 Not Found/SERVEFILE</h1></body></html>";
		response.headers["Content-Type"] = "text/html";
		response.headers["Content-Length"] = "47";
		std::cout << "Error trying to find : " << path << std::endl;
		return (response);
	}
	char	buf[4096];
	ssize_t bytes;
	while ((bytes = read(fd, buf, sizeof(buf))) > 0)
		response.body.append(buf, bytes);
	close(fd);

	std::ostringstream oss;
	oss << response.body.size();

	response.statusCode    = 200;
	response.statusMessage = "OK";
	response.headers["Content-Type"]   = getMimeType(path);
	response.headers["Content-Length"] = oss.str();
	return (response);
}

HttpResponse	execute(HttpRequest const &req, LocationConfig const &loc, ServerConfig const &server)
{
	bool			allowed;
	std::string		path;
	HttpResponse	response;

	allowed = false;
	for (size_t i = 0; i < loc.methods.size(); i++){
		if (loc.methods[i] == req.method)
			allowed = true;
	}
	if (!allowed){
		response.statusCode = 405;
		response.statusMessage = "method not allowed";
		return (response);
	}
	path = resolvePath(req, loc);
	std::cout << "Method called : " << req.method << std::endl;
	if (req.method == "GET")
		return (handleGet(req, loc, path, server));
	else if (req.method == "POST")
		return (handlePost(req, loc));
	else if (req.method == "DELETE")
		return (handleDelete(path));
	return (response);
}
