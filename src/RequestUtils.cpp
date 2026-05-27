/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestUtils.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: cjauregu <cjauregu@student.42lausanne.c    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/22 16:31:11 by lylrandr          #+#    #+#             */
/*   Updated: 2026/05/27 09:31:55 by cjauregu         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpHandler.hpp"
#include "CGI.hpp"

// returns true if a complete request was parsed; consumed is set to bytes consumed
bool parseRequestFromBuffer(const std::string &buf, HttpRequest &outReq, size_t &consumed) {
    consumed = 0;
    size_t pos = buf.find("\r\n\r\n");
    if (pos == std::string::npos) {
        return false;
    }
    size_t headerEnd = pos + 4;

    std::string headerBlock = buf.substr(0, headerEnd);
    size_t clPos = std::string::npos;
    clPos = headerBlock.find("Content-Length:");
    size_t contentLength = 0;
    if (clPos != std::string::npos) {
        clPos += strlen("Content-Length:");
        while (clPos < headerBlock.size() && headerBlock[clPos] == ' ')
            ++clPos;
        contentLength = static_cast<size_t>(std::atoi(headerBlock.c_str() + clPos));
    } else {
        contentLength = 0;
    }
    size_t totalNeeded = headerEnd + contentLength;
    if (buf.size() < totalNeeded) {
        return false;
    }
    std::string requestSlice = buf.substr(0, totalNeeded);
    outReq = parseRequest(requestSlice);
    consumed = totalNeeded;
    return true;
}

HttpResponse	isDir(LocationConfig const &location, std::string path, HttpResponse response){
	std::ostringstream	oss;
	std::string			name;
	DIR					*dir;
	dirent				*entry;

	if (location.autoindex){
	dir = opendir(path.c_str());
	if (dir == NULL){
		response.statusCode = 500;
		response.statusMessage = "Internal server error";
		return(response);
	}
	response.body = "<html><body><h1>Index of :" + path + "</h1><ul>";
	while ((entry = readdir(dir)) != NULL){
		name = entry->d_name;
		if (name == "." || name == "..")
			continue;
		response.body += "<li><a href=\"" + name + "\">" + name + "</a></li>";
	}
	response.body += "</ul></body></html>";
	oss << response.body.size();
	response.headers["Content-Type"] = "text/html";
	response.headers["Content-Length"] = oss.str();
	closedir(dir);
	response.statusCode = 200;
	response.statusMessage = "OK";
	return(response);
	}
	else{
		response.statusCode = 403;
		response.statusMessage = "Forbidden";
		return (response);
	}
}

HttpResponse	handleGet(HttpRequest const &req, LocationConfig const &location, std::string path, ServerConfig const &server)
{
	HttpResponse		response;
	struct stat			fileInfo;

	std::cout << "GET/Path is named : " << path << std::endl;
	if (stat(path.c_str(), &fileInfo) < 0){
		response.statusCode = 404;
		response.statusMessage = "Not found/GET";
		return(response);
	}
	if (S_ISDIR(fileInfo.st_mode))
		return (isDir(location, path, response));
    else if (isCGIvalid(location, path))
	{
		try
		{
			response = runCGI(req, server, location, path);
		}
		catch (std::exception &e)
		{
			response.statusMessage = "CGI failure";
			response.statusCode = 403;
			return (response);
		}
	}
	else if (S_ISREG(fileInfo.st_mode)){
		if (access(path.c_str(), R_OK) < 0){
			response.statusCode = 403;
			response.statusMessage = "Forbidden";
			response.body = "403 Forbidden";
			response.headers["Content-Type"] = "text/plain";
			response.headers["Content-Length"] = std::to_string(response.body.size());
			return response;
		}
		return (serveFile(path));
	}
	return (response);
}

//verifier max body size --> 413
HttpResponse	handlePost(HttpRequest const &request, LocationConfig const &location){
	HttpResponse	response;
	std::string		contentType;
	std::string		boundary;
	std::string		filename;
	std::string		content;
	std::string		filepath;
	size_t			pos;
	int				fd;

	contentType = request.headers.at("Content-Type");
	pos = contentType.find("boundary=");
	boundary = request.body.substr(pos + 9);
	pos = request.body.find("filename=\"");
	filename = request.body.substr(pos + 10);
	filename = filename.substr(0, filename.find("\""));
	pos = request.body.find("Content-Type:");
	pos =  request.body.find("\r\n\r\n", pos);
	content = request.body.substr(pos);
	content = content.substr(0, content.find("--"));
	if (content.size() >= 2 && content[0] == '\r' && content[1] == '\n')
		content = content.substr(2);
	if (content.size() >= 2 && content[content.size()-2] == '\r' && content[content.size()-1] == '\n')
		content = content.substr(0, content.size() - 2);
	filepath = location.upload_store + '/' + filename;
	fd = open(filepath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0){
		response.statusCode = 500;
		response.statusMessage = "Internal Server Error";
		return (response);
	}
	write(fd, content.c_str(), content.size());
	close(fd);
	response.statusCode = 201;
	response.statusMessage = "Created";
	return (response);
}

HttpResponse makeError(int code, const std::string& msg) {
    HttpResponse r;
    r.statusCode = code;
    r.statusMessage = msg;
    r.body = "<html><body><h1>" + std::to_string(code) + " " + msg + "</h1></body></html>";
    r.headers["Content-Type"] = "text/html";
    r.headers["Content-Length"] = std::to_string(r.body.size());
    return r;
}

HttpResponse handleDelete(const std::string& path)
{
    HttpResponse res;

    struct stat st;
    if (stat(path.c_str(), &st) < 0) {
		return makeError(404, "Not found");
    }
	if (S_ISDIR(st.st_mode)) {
		return makeError(403, "Forbidden");
	}
    if (access(path.c_str(), W_OK) == -1) {
		return makeError(403, "Forbidden");
    }
    if (unlink(path.c_str()) == -1) {
		return makeError(500, "Not found");
    }
    res.statusCode = 204;
    res.statusMessage = "Success";
    return res;
}
