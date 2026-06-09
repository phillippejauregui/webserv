/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestUtils.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/22 16:31:11 by lylrandr          #+#    #+#             */
/*   Updated: 2026/06/08 18:54:45 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/HttpHandler.hpp"
#include "CGI.hpp"

HttpResponse buildError(int code, std::string const &message, ServerConfig const &config){
	HttpResponse		response;
	std::ostringstream	oss;
	std::string			fullpath;

	response.statusCode    = code;
	response.statusMessage = message;
	response.headers["Content-Type"] = "text/html";
	std::cout << "map size: " << config.error_page.size() << std::endl;
	for (std::map<int, std::string>::const_iterator it = config.error_page.begin(); it != config.error_page.end(); it++)
		std::cout << "code: " << it->first << " path: " << it->second << std::endl;
	if (config.error_page.count(code)){
		fullpath = config.root + config.error_page.at(code);
		std::cout << "fullpath: [" << fullpath << "]" << std::endl;
		int fd = open(fullpath.c_str(), O_RDONLY);
		if (fd >= 0){
			char	buf[4096];
			ssize_t	bytes;
			while ((bytes = read(fd, buf, sizeof(buf))) > 0)
				response.body.append(buf, bytes);
			close(fd);
			oss << response.body.size();
			response.headers["Content-Length"] = oss.str();
			return (response);
		}
	}
	oss << code;
	response.body = "<html><body><h1>" + oss.str() + " " + message + "</h1></body></html>";
	oss.str("");
	oss << response.body.size();
	response.headers["Content-Length"] = oss.str();
	return (response);
}

bool parseRequestFromBuffer(const std::string &buf, HttpRequest &outReq, size_t &consumed) {
    consumed = 0;

    size_t pos = buf.find("\r\n\r\n");
    if (pos == std::string::npos)
        return false;

    size_t headerEnd = pos + 4;
    std::string headerBlock = buf.substr(0, headerEnd);
    size_t contentLength = 0;
    size_t clPos = headerBlock.find("Content-Length:");
    if (clPos != std::string::npos) {
        clPos += strlen("Content-Length:");
        while (clPos < headerBlock.size() && headerBlock[clPos] == ' ')
            ++clPos;
        contentLength = static_cast<size_t>(std::atoi(headerBlock.c_str() + clPos));
    }

    size_t totalNeeded = headerEnd + contentLength;
    if (buf.size() < totalNeeded)
        return false;
    std::string requestSlice = buf.substr(0, totalNeeded);
    outReq = parseRequest(requestSlice, headerEnd, contentLength);
    consumed = totalNeeded;
    return true;
}

static std::string toString(size_t n) {
    std::stringstream ss;
    ss << n;
    return ss.str();
}

HttpResponse isDir(LocationConfig const &location,
                   std::string path,
                   HttpResponse response)
{
    std::ostringstream oss;
    std::string        name;
    DIR               *dir;
    dirent            *entry;

    if (location.autoindex) {
        dir = opendir(path.c_str());
        if (dir == NULL) {
            response.statusCode = 500;
            response.statusMessage = "Internal Server Error";
            response.body = "<h1>500 Internal Server Error</h1>";
            response.headers["Content-Type"] = "text/html";
            response.headers["Content-Length"] =
                toString(response.body.size());
            return response;
        }
        response.body = "<html><body><h1>Index of: " + path + "</h1><ul>";
        while ((entry = readdir(dir)) != NULL) {
            name = entry->d_name;
            if (name == "." || name == "..")
                continue;
            response.body += "<li><a href=\"" + name + "\">" + name + "</a></li>";
        }
        response.body += "</ul></body></html>";
        closedir(dir);

        response.statusCode = 200;
        response.statusMessage = "OK";
        response.headers["Content-Type"] = "text/html";
        response.headers["Content-Length"] =
            toString(response.body.size());
        return response;
    } else {
        response.statusCode = 403;
        response.statusMessage = "Forbidden";
        response.body = "<h1>403 Forbidden1</h1>";
        response.headers["Content-Type"] = "text/html";
        response.headers["Content-Length"] =
            toString(response.body.size());
        return response;
    }
}

HttpResponse handleGet(LocationConfig const &location, std::string path)
{
    HttpResponse response;
    struct stat  fileInfo;

    if (stat(path.c_str(), &fileInfo) < 0) {
        response.statusCode = 404;
        response.statusMessage = "Not Found";
        response.body = "<h1>404 Not Found</h1>";
        response.headers["Content-Type"] = "text/html";
        response.headers["Content-Length"] =
            toString(response.body.size());
        return response;
    }
    if (S_ISDIR(fileInfo.st_mode)) {
        return isDir(location, path, response);
    } else if (S_ISREG(fileInfo.st_mode)) {
        if (access(path.c_str(), R_OK) < 0) {
            response.statusCode = 403;
            response.statusMessage = "Forbidden";
            response.body = "<h1>403 Forbidden</h1>";
            response.headers["Content-Type"] = "text/html";
            response.headers["Content-Length"] =
                toString(response.body.size());
            return response;
        }
        return serveFile(path);
    }
    response.statusCode = 404;
    response.statusMessage = "Not Found";
    response.body = "<h1>404 Not Found</h1>";
    response.headers["Content-Type"] = "text/html";
    response.headers["Content-Length"] =
        toString(response.body.size());
    return response;
}

HttpResponse handlePost(const HttpRequest& request,
                        const LocationConfig& location,
                        const ServerConfig& server,
                        const std::string& path)
{
    (void)server;
    (void)path;
    if (!location.upload_enabled)
        return buildError(400, "Uploads not enabled for this location", server);
    std::map<std::string, std::string>::const_iterator it =
        request.headers.find("Content-Type");
    if (it == request.headers.end())
        return buildError(400, "Missing Content-Type", server);
    const std::string &contentType = it->second;
    size_t bpos = contentType.find("boundary=");
    if (bpos == std::string::npos)
        return buildError(400, "Missing boundary in Content-Type", server);
    std::string boundary = contentType.substr(bpos + 9);
    while (!boundary.empty()) {
        char c = boundary[boundary.size() - 1];
        if (c == '\r' || c == '\n' || c == ';' || c == ' ')
            boundary.erase(boundary.size() - 1);
        else
            break;
    }
    boundary = "--" + boundary;
    return parseMultipartAndSave(request.body, boundary, location, server);
}


HttpResponse handleDelete(const std::string& path, const ServerConfig& server)
{
    HttpResponse res;

    struct stat st;
    if (stat(path.c_str(), &st) < 0) {
		return buildError(404, "Not found", server);
    }
	if (S_ISDIR(st.st_mode)) {
		return buildError(403, "Forbidden", server);
	}
    if (access(path.c_str(), W_OK) == -1) {
		return buildError(403, "Forbidden", server);
    }
    if (unlink(path.c_str()) == -1) {
		return buildError(500, "Not found", server);
    }
    makeUploadResponse(204, "Success", "");
    return res;
}
