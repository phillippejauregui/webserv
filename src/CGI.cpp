#include "CGI.hpp"

std::string getExtension(const std::string &path) {
    size_t pos = path.rfind('.');
    if (pos == std::string::npos)
        return "";
    std::string ext = path.substr(pos);
    while (!ext.empty() && (ext.back() == '\r' || ext.back() == '\n' || ext.back() == ' '))
        ext.pop_back();
    return ext;
}

bool isCGIvalid(LocationConfig const &loc, std::string const &fullPath)
{
    std::string path;
    std::string ext;
    size_t q = fullPath.find('?');
    if (q != std::string::npos)
    {
        path = fullPath.substr(0, q);
        ext = getExtension(path);
    }
    else
        ext = getExtension(fullPath);
    if (ext.empty())
        return false;
    std::map<std::string, std::string>::const_iterator it = loc.cgi.find(ext);
    if (it == loc.cgi.end())
        return false;
    return !it->second.empty();
}

CGIEnv buildCGIEnv(HttpRequest const &req, ServerConfig const &server, std::string const &fullPath)
{
    CGIEnv env;

    env.requestMethod = req.method;
    size_t pos = req.uri.find('?');
    env.queryString = (pos != std::string::npos ? req.uri.substr(pos + 1) : "");
    env.contentLength = (req.method == "POST" ? std::to_string(req.body.size()) : "0");
    std::map<std::string, std::string>::const_iterator ct = req.headers.find("Content-Type");
    env.contentType = (ct != req.headers.end()) ? ct->second : "";
    env.serverProtocol = req.version;
    env.scriptFilename = fullPath;
    env.scriptName = req.uri;
    env.serverName = server.server_name;
    env.serverPort = std::to_string(server.listen);
    env.gatewayInterface = "CGI/1.1";

    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin(); it != req.headers.end(); ++it)
    {
        std::string key = it->first;
        for (size_t i = 0; i < key.size(); i++)
            key[i] = (key[i] == '-' ? '_' : std::toupper(key[i]));
        env.httpHeaders["HTTP_" + key] = it->second;
    }
    return env;
}

char** buildENVP(const CGIEnv &env)
{
    std::vector<std::string> vars;

    vars.push_back("REQUEST_METHOD=" + env.requestMethod);
    vars.push_back("QUERY_STRING=" + env.queryString);
    vars.push_back("CONTENT_LENGTH=" + env.contentLength);
    vars.push_back("CONTENT_TYPE=" + env.contentType);
    vars.push_back("SERVER_PROTOCOL=" + env.serverProtocol);
    vars.push_back("SCRIPT_FILENAME=" + env.scriptFilename);
    vars.push_back("SCRIPT_NAME=" + env.scriptName);
    vars.push_back("SERVER_NAME=" + env.serverName);
    vars.push_back("SERVER_PORT=" + env.serverPort);
    vars.push_back("GATEWAY_INTERFACE=" + env.gatewayInterface);
    vars.push_back("PYTHONUNBUFFERED=1");
    for (std::map<std::string, std::string>::const_iterator it = env.httpHeaders.begin();
         it != env.httpHeaders.end(); ++it)
    {
        vars.push_back(it->first + "=" + it->second);
    }
    char **envp = new char*[vars.size() + 1];
    for (size_t i = 0; i < vars.size(); i++)
        envp[i] = strdup(vars[i].c_str());
    envp[vars.size()] = NULL;
    return envp;
}

void runCGIChild(const LocationConfig &loc, const std::string &fullPath, int inPipe[2], int outPipe[2], char** envp)
{
    close(inPipe[1]);
    close(outPipe[0]);

    if (dup2(inPipe[0], STDIN_FILENO) == -1)
        _exit(1);
    if (dup2(outPipe[1], STDOUT_FILENO) == -1)
        _exit(1);
    close(inPipe[0]);
    close(outPipe[1]);
    std::string ext = getExtension(fullPath);
    std::map<std::string, std::string>::const_iterator it = loc.cgi.find(ext);
    if (it == loc.cgi.end())
        _exit(1);
    std::string interpreter = it->second;
    char *argv[3];
    argv[0] = const_cast<char*>(interpreter.c_str());
    argv[1] = const_cast<char*>(fullPath.c_str());
    argv[2] = NULL;
    execve(interpreter.c_str(), argv, envp);
    _exit(1);
}


std::string runCGIParent(const HttpRequest &req, int inPipe[2], int outPipe[2], pid_t pid)
{
    close(inPipe[0]);
    close(outPipe[1]);
    if (req.method == "POST" && !req.body.empty())
    {
        ssize_t written = 0;
        const std::string& body = req.body;
        while (written < (ssize_t)body.size())
        {
            ssize_t n = write(inPipe[1], body.c_str() + written, body.size() - written);
            if (n <= 0)
                break;
            written += n;
        }
    }
    close(inPipe[1]);
    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(outPipe[0], buf, sizeof(buf))) > 0)
        output.append(buf, n);
    close(outPipe[0]);
    int status;
    waitpid(pid, &status, 0);
    return output;
}

HttpResponse parseCGIOutput(const std::string &raw)
{
    HttpResponse res;

    size_t pos = raw.find("\r\n\r\n");
    size_t sepLen = 4;
    if (pos == std::string::npos) {
        pos = raw.find("\n\n");
        sepLen = 2;
    }
    if (pos == std::string::npos) {
        res.statusCode = 500;
        res.statusMessage = "Internal Server Error";
        res.body = "Invalid CGI output";
        return res;
    }
    std::string headerPart = raw.substr(0, pos);
    std::string bodyPart = raw.substr(pos + sepLen);
    res.body = bodyPart;
    std::istringstream stream(headerPart);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        if (!value.empty() && value[0] == ' ')
            value.erase(0, 1);
        if (key == "Status")
        {
            std::istringstream ss(value);
            ss >> res.statusCode;
            std::getline(ss, res.statusMessage);
            if (!res.statusMessage.empty() && res.statusMessage[0] == ' ')
                res.statusMessage.erase(0, 1);
            continue;
        }
        res.headers[key] = value;
    }
    if (res.statusCode == 0)
    {
        res.statusCode = 200;
        res.statusMessage = "OK";
    }
    if (res.headers.find("Content-Length") == res.headers.end()) {
        res.headers["Content-Length"] = std::to_string(res.body.size());
    }
    return res;
}

HttpResponse runCGI(const HttpRequest &req, const ServerConfig &server, const LocationConfig &loc, const std::string &fullPath)
{
    CGIEnv env = buildCGIEnv(req, server, fullPath);
    char** envp = buildENVP(env);

    int inPipe[2];
    int outPipe[2];
    if (pipe(inPipe) == -1 || pipe(outPipe) == -1)
    {
        for (size_t i = 0; envp[i]; ++i)
            free(envp[i]);
        delete [] envp;
        throw std::runtime_error("Pipe Failed\n");
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        for (size_t i = 0; envp[i]; ++i)
            free(envp[i]);
        delete [] envp;
        throw std::runtime_error("Piping error\n");
    }
    if (pid == 0)
    {
        try
        {
            runCGIChild(loc, fullPath, inPipe, outPipe, envp);
        }
        catch (std::exception &e)
        {
            for (size_t i = 0; envp[i]; ++i)
                free(envp[i]);
            delete [] envp;
            throw std::runtime_error("Child Process failed");
        }
    }
    std::string cgi_output = runCGIParent(req, inPipe, outPipe, pid);
    for (size_t i = 0; envp[i]; ++i)
        free(envp[i]);
    delete [] envp;
    return parseCGIOutput(cgi_output);
}
