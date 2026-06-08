#include "CGI.hpp"
#include <stdint.h>
#include <sstream>
#include <cstdlib>
#include <cstring>

static std::string toString(size_t n) {
    std::stringstream ss;
    ss << n;
    return ss.str();
}

std::string getExtension(const std::string &path) {
    size_t pos = path.rfind('.');
    if (pos == std::string::npos)
        return "";
    std::string ext = path.substr(pos);
 
    while (!ext.empty()) {
        char c = ext[ext.size() - 1];
        if (c == '\r' || c == '\n' || c == ' ' || c == ';')
            ext.erase(ext.size() - 1);
        else
            break;
    }
    return ext;
}

bool isCGIvalid(const LocationConfig &loc, const std::string &fullPath)
{
    std::string ext;
    size_t q = fullPath.find('?');
    if (q != std::string::npos)
        ext = getExtension(fullPath.substr(0, q));
    else
        ext = getExtension(fullPath);
 
    if (ext.empty())
        return false;
 
    std::map<std::string, std::string>::const_iterator it = loc.cgi.find(ext);
    return (it != loc.cgi.end() && !it->second.empty());
}

CGIEnv buildCGIEnv(const HttpRequest &req,
                   const ServerConfig &server,
                   const std::string &fullPath)
{
    CGIEnv env;
 
    size_t qpos = req.uri.find('?');
    std::string uriPath  = (qpos != std::string::npos ? req.uri.substr(0, qpos) : req.uri);
    std::string queryStr = (qpos != std::string::npos ? req.uri.substr(qpos + 1) : "");
 
    std::map<std::string, std::string>::const_iterator ct = req.headers.find("Content-Type");
    std::string contentType = (ct != req.headers.end()) ? ct->second : "";
 
    if (req.method == "POST" &&
        queryStr.empty() &&
        contentType.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        queryStr = req.body;
    }
 
    env.requestMethod    = req.method;
    env.queryString      = queryStr;
    env.contentLength    = (req.method == "POST" ? toString(req.body.size()) : "0");
    env.contentType      = contentType;
    env.serverProtocol   = req.version;
    env.scriptFilename   = fullPath;
    env.scriptName       = uriPath;
    env.serverName       = server.server_name;
    env.serverPort       = toString(server.listen);
    env.gatewayInterface = "CGI/1.1";
 
    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin();
         it != req.headers.end(); ++it)
    {
        std::string key = it->first;
        for (size_t i = 0; i < key.size(); i++)
            key[i] = (key[i] == '-' ? '_' : std::toupper(key[i]));
        env.httpHeaders["HTTP_" + key] = it->second;
    }
 
    /*std::cerr << "[CGI] ENV prepared:\n";
    std::cerr << "  SCRIPT_FILENAME=" << env.scriptFilename << "\n";
    std::cerr << "  REQUEST_METHOD=" << env.requestMethod << "\n";
    std::cerr << "  QUERY_STRING=" << env.queryString << "\n";
    std::cerr << "  CONTENT_LENGTH=" << env.contentLength << "\n";
    std::cerr << "  CONTENT_TYPE=" << env.contentType << "\n";*/
 
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
        vars.push_back(it->first + "=" + it->second);
 
    char **envp = new char*[vars.size() + 1];
    for (size_t i = 0; i < vars.size(); i++)
        envp[i] = strdup(vars[i].c_str());
    envp[vars.size()] = NULL;
 
    return envp;
}
 
void runCGIChild(const LocationConfig &loc, const std::string &fullPath,
                 int inPipe[2], int outPipe[2], char** envp)
{
    //std::cerr << "[CGI CHILD] start, fullPath=" << fullPath << "\n";
    close(inPipe[1]);
    close(outPipe[0]);
    if (dup2(inPipe[0], STDIN_FILENO) == -1) {
        //std::cerr << "[CGI CHILD] dup2(stdin) failed: " << strerror(errno) << "\n";
        _exit(1);
    }
    if (dup2(outPipe[1], STDOUT_FILENO) == -1) {
        //std::cerr << "[CGI CHILD] dup2(stdout) failed: " << strerror(errno) << "\n";
        _exit(1);
    }
    close(inPipe[0]);
    close(outPipe[1]);
    std::string ext = getExtension(fullPath);
    //std::cerr << "[CGI CHILD] ext=" << ext << "\n";
    std::map<std::string, std::string>::const_iterator it = loc.cgi.find(ext);
    if (it == loc.cgi.end()) {
        //std::cerr << "[CGI CHILD] no interpreter for ext\n";
        _exit(1);
    }
    const char *interp = it->second.c_str();
    //std::cerr << "[CGI CHILD] interp=" << interp << "\n";
    std::string scriptDir  = ".";
    std::string scriptFile = fullPath;
    size_t lastSlash = fullPath.rfind('/');
    if (lastSlash != std::string::npos) {
        scriptDir  = fullPath.substr(0, lastSlash);
        scriptFile = fullPath.substr(lastSlash + 1);
    }
    if (chdir(scriptDir.c_str()) == -1) {
        //std::cerr << "[CGI CHILD] chdir failed: " << strerror(errno) << "\n";
        _exit(1);
    }
    //std::cerr << "[CGI CHILD] chdir to " << scriptDir << ", script=" << scriptFile << "\n";
 
    char *argv[3];
    argv[0] = const_cast<char*>(interp);
    argv[1] = const_cast<char*>(scriptFile.c_str());
    argv[2] = NULL;
 
    //std::cerr << "[CGI CHILD] execve()...\n";
    execve(interp, argv, envp);
 
    //std::cerr << "[CGI CHILD] execve FAILED: " << strerror(errno) << "\n";
    _exit(1);
}
 
 
std::string runCGIParent(const HttpRequest &req, int inPipe[2], int outPipe[2], pid_t pid)
{
    //std::cerr << "[CGI PARENT] start, pid=" << pid << "\n";
 
    close(inPipe[0]);
    close(outPipe[1]);
 
    if (req.method == "POST" && !req.body.empty())
    {
        ssize_t written = 0;
        const std::string &body = req.body;
        //std::cerr << "[CGI PARENT] writing POST body, size=" << body.size() << "\n";
 
        while (written < (ssize_t)body.size())
        {
            ssize_t n = write(inPipe[1], body.c_str() + written, body.size() - written);
            //std::cerr << "[CGI PARENT] write() n=" << n << "\n";
            if (n <= 0) {
                //std::cerr << "[CGI PARENT] write error: " << strerror(errno) << "\n";
                break;
            }
            written += n;
        }
    }
    else {
        //std::cerr << "[CGI PARENT] no body to write\n";
    }
 
    close(inPipe[1]);
    //std::cerr << "[CGI PARENT] stdin closed, reading stdout...\n";
 
    std::string output;
    char buf[4096];
    ssize_t n;
    //size_t total = 0;
 
    while ((n = read(outPipe[0], buf, sizeof(buf))) > 0) {
        output.append(buf, n);
        //total += n;
        //std::cerr << "[CGI PARENT] read() n=" << n << ", total=" << total << "\n";
    }
 
    /*if (n < 0)
        std::cerr << "[CGI PARENT] read error: " << strerror(errno) << "\n";
    else
        std::cerr << "[CGI PARENT] EOF on CGI stdout\n";*/
 
    close(outPipe[0]);
 
    int status = 0;
    waitpid(pid, &status, 0);
    //std::cerr << "[CGI PARENT] child exit status=" << status << "\n";
    //std::cerr << "[CGI PARENT] total output size=" << output.size() << "\n";
 
    return output;
}
 
 
HttpResponse parseCGIOutput(const std::string &raw)
{
    HttpResponse res;
 
    //std::cerr << "[CGI PARSE] raw size=" << raw.size() << "\n";
 
    size_t pos = raw.find("\r\n\r\n");
    size_t sepLen = 4;
 
    if (pos == std::string::npos) {
        pos = raw.find("\n\n");
        sepLen = 2;
    }
 
    if (pos == std::string::npos) {
        //std::cerr << "[CGI PARSE] no header/body separator found\n";
        res.statusCode = 500;
        res.statusMessage = "Internal Server Error";
        res.body = "Invalid CGI output";
        return res;
    }
 
    std::string headerPart = raw.substr(0, pos);
    std::string bodyPart   = raw.substr(pos + sepLen);
 
    //std::cerr << "[CGI PARSE] header size=" << headerPart.size()
             // << " body size=" << bodyPart.size() << "\n";
 
    res.body = bodyPart;
 
    std::istringstream stream(headerPart);
    std::string line;
 
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
 
        //std::cerr << "[CGI PARSE] header line: '" << line << "'\n";
 
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;
 
        std::string key   = line.substr(0, colon);
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
 
            //std::cerr << "[CGI PARSE] Status=" << res.statusCode
                      //<< " '" << res.statusMessage << "'\n";
            continue;
        }
 
        res.headers[key] = value;
        //std::cerr << "[CGI PARSE] header: " << key << " = " << value << "\n";
    }
    if (res.statusCode == 0) {
        res.statusCode = 200;
        res.statusMessage = "OK";
    }
    if (res.headers.find("Content-Length") == res.headers.end()) {
        res.headers["Content-Length"] = toString(res.body.size());
        //std::cerr << "[CGI PARSE] auto Content-Length=" << res.body.size() << "\n";
    }
    return res;
}
 
 
CGIProcess launchCGI(const HttpRequest &req,
                     const ServerConfig &server,
                     const LocationConfig &loc,
                     const std::string &fullPath)
{
    //std::cerr << "[CGI] launchCGI fullPath=" << fullPath << "\n";
 
    CGIEnv env = buildCGIEnv(req, server, fullPath);
    char** envp = buildENVP(env);
 
    int inPipe[2];
    int outPipe[2];
 
    if (pipe(inPipe) == -1 || pipe(outPipe) == -1) {
        //std::cerr << "[CGI] pipe() failed: " << strerror(errno) << "\n";
        for (size_t i = 0; envp[i]; ++i) free(envp[i]);
        delete[] envp;
        throw std::runtime_error("pipe() failed");
    }
 
    setNonBlocking(inPipe[1]);
    setNonBlocking(outPipe[0]);
 
    //std::cerr << "[CGI] pipes: in=" << inPipe[0] << "," << inPipe[1]
              //<< " out=" << outPipe[0] << "," << outPipe[1] << "\n";
 
    pid_t pid = fork();
    if (pid < 0) {
        //std::cerr << "[CGI] fork() failed: " << strerror(errno) << "\n";
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        for (size_t i = 0; envp[i]; ++i) free(envp[i]);
        delete[] envp;
        throw std::runtime_error("fork() failed");
    }
 
    if (pid == 0) {
        runCGIChild(loc, fullPath, inPipe, outPipe, envp);
    }
 
    //std::cerr << "[CGI] fork ok, pid=" << pid << "\n";
 
    close(inPipe[0]);
    close(outPipe[1]);
 
    for (size_t i = 0; envp[i]; ++i) free(envp[i]);
    delete[] envp;
 
    CGIProcess cgi;
    cgi.pid         = pid;
    cgi.inFd        = inPipe[1];
    cgi.outFd       = outPipe[0];
    cgi.inputBuffer = req.body;
    cgi.inputOffset = 0;
    cgi.inputDone   = req.body.empty();
    cgi.outputDone  = false;
    cgi.startTime   = time(NULL);
 
    //std::cerr << "[CGI] launchCGI done, inputDone=" << cgi.inputDone << "\n";
 
    return cgi;
}
