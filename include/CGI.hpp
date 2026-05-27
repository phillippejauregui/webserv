#ifndef SCRIPT_HPP
# define SCRIPT_HPP

#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "PollServer.hpp"

struct CGIEnv {
    std::string requestMethod;
    std::string queryString;
    std::string contentLength;
    std::string contentType;
    std::string serverProtocol;
    std::string scriptFilename;
    std::string scriptName;
    std::string serverName;
    std::string serverPort;
    std::string gatewayInterface;
    std::map<std::string, std::string> httpHeaders;
};

bool	isCGIvalid(LocationConfig const &loc, std::string const &fullPath);
HttpResponse runCGI(const HttpRequest &req, const ServerConfig &server, const LocationConfig &loc, const std::string &fullPath);


#endif