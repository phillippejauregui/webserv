#include "Parser.hpp"
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

ConfigParser::ConfigParser(int argc, char** argv)
{
	if (argc != 2)
		throw std::runtime_error("Incorrect arguments\n");
	filename = argv[1];
}

const std::vector<ServerConfig>& ConfigParser::getServers() const
{
	return servers;
}

static std::string toString(size_t n) {
    std::stringstream ss;
    ss << n;
    return ss.str();
}

std::vector<std::string> tokenize(const std::string& text)
{
	std::vector<std::string> tokens;
	std::string current;

	for (size_t i = 0; i < text.size(); ++i)
	{
		char c = text[i];
		if (isspace(c))
		{
			if (!current.empty())
			{
				tokens.push_back(current);
				current.clear();
			}
			continue;
		}
		if (c == '{' || c == '}' || c == ';')
		{
			if (!current.empty())
			{
				tokens.push_back(current);
				current.clear();
			}
			tokens.push_back(std::string(1, c));
			continue;
		}
		current += c;
	}
	if (!current.empty())
		tokens.push_back(current);
	return tokens;
}

LocationConfig parseLocation(const std::vector<std::string> &tokens, size_t &i)
{
    LocationConfig loc;
    loc.autoindex = false;
    loc.upload_enabled = false;

    if (tokens[i] != "location")
        throw std::runtime_error("Expected 'location'");
    ++i;
    loc.path = tokens[i++];
    if (tokens[i] != "{")
        throw std::runtime_error("Expected '{' after location");
    ++i;
    while (tokens[i] != "}") {

        std::string key = tokens[i++];
        if (key == "allow_methods") {
            while (tokens[i] != ";") {
                loc.methods.push_back(tokens[i]);
                ++i;
            }
            ++i;
            continue;
        }
        if (key == "cgi") {
            std::string ext = tokens[i++];
            std::string interpreter = tokens[i++];
            ++i;
            loc.cgi[ext] = interpreter;
            continue;
        }
        if (key == "upload_store") {
            std::string value = tokens[i++];
            loc.upload_store = value;
            ++i;
            loc.upload_enabled = true;
            continue;
        }
        std::string value = tokens[i++];
        //std::cout << key << '\n' << value << std::endl;
        if (tokens[i] != ";")
            throw std::runtime_error("Expected ';'");
        ++i;
        if (key == "root") loc.root = value;
        else if (key == "index") loc.index = value;
        else if (key == "autoindex") loc.autoindex = (value == "on");
        else if (key == "return") loc.redirect = value;
        else
            throw std::runtime_error("Unknown directive in location: " + key);
    }
    ++i;
    return loc;
}

void ConfigParser::parseTokens(const std::vector<std::string> &tokens) {
	size_t i = 0;

    while (i < tokens.size()) {
        if (tokens[i] != "server")
            throw std::runtime_error("Expected 'server'");
        ++i;
        if (i >= tokens.size() || tokens[i] != "{")
            throw std::runtime_error("Expected '{' after server");
        ++i;

        ServerConfig cfg;
        cfg.listen = 0;
        cfg.client_max_body_size = 0;

        while (i < tokens.size() && tokens[i] != "}") {
            if (tokens[i] == "location")
            {
                LocationConfig loc = parseLocation(tokens, i);
                cfg.locations.push_back(loc);
                continue;
            }
            std::string key = tokens[i++];
            if (i >= tokens.size())
                throw std::runtime_error("Unexpected end of file");
            std::string value = tokens[i++];
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';'");
            ++i;
            if (key == "server_name") cfg.server_name = value;
            else if (key == "listen") cfg.listen = std::atoi(value.c_str());
            else if (key == "host") cfg.host = value;
            else if (key == "root") cfg.root = value;
            else if (key == "index") cfg.index = value;
            else if (key == "client_max_body_size") cfg.client_max_body_size = std::strtoul(value.c_str(), 0, 10);
            else if (key == "error_page") cfg.error_page = value;
            else
                throw std::runtime_error("Unknown directive: " + key);
        }
        if (i >= tokens.size() || tokens[i] != "}")
            throw std::runtime_error("Expected '}' at end of server block");
        ++i;
        servers.push_back(cfg);
    }
}

void ConfigParser::parse()
{
	int fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0)
		throw std::runtime_error("Cannot open config file");

	std::string text;
	char buf[4096];
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		text.append(buf, n);
	}
	close(fd);
	if (n < 0)
		throw std::runtime_error("Error reading config file");
	std::vector<std::string> tokens = tokenize(text);
	parseTokens(tokens);
}

HttpResponse makeUploadResponse(int status,
                                const std::string& message,
                                const std::string& extra)
{
    HttpResponse res;

    res.statusCode = status;

    switch (status) {
        case 201: res.statusMessage = "Created"; break;
        default:  res.statusMessage = "OK"; break;
    }
    std::string body =
        "<html><body>"
        "<h1>" + toString(status) + " " + res.statusMessage + "</h1>"
        "<p>" + message + "</p>";
    if (!extra.empty())
        body += "<pre>" + extra + "</pre>";
    body += "</body></html>";
    res.body = body;
    res.headers["Content-Type"] = "text/html";
    res.headers["Content-Length"] = toString(res.body.size());
    return res;
}

HttpResponse parseMultipartAndSave(const std::string& body,
                                   const std::string& boundary,
                                   const LocationConfig& location,
                                   const ServerConfig& server)
{
    size_t start = body.find(boundary);
    if (start == std::string::npos)
        return buildError(400, "Start boundary not found", server);
    start += boundary.size();
    if (start + 1 < body.size() && body[start] == '\r' && body[start + 1] == '\n')
        start += 2;
    size_t headerEnd = body.find("\r\n\r\n", start);
    if (headerEnd == std::string::npos)
        return buildError(400, "Malformed multipart body", server);
    std::string partHeaders = body.substr(start, headerEnd - start);
    size_t contentStart = headerEnd + 4;
    std::string endBoundary = "\r\n" + boundary + "--";
    size_t end = body.find(endBoundary, contentStart);
    if (end == std::string::npos)
        return buildError(400, "End boundary not found", server);
    const char* contentPtr = body.data() + contentStart;
    size_t contentSize = end - contentStart;
    size_t fn = partHeaders.find("filename=\"");
    if (fn == std::string::npos)
        return buildError(400, "Missing filename", server);
    size_t fnStart = fn + 10;
    size_t fnEnd = partHeaders.find("\"", fnStart);
    if (fnEnd == std::string::npos)
        return buildError(400, "Malformed filename", server);
    std::string filename = partHeaders.substr(fnStart, fnEnd - fnStart);
    if (filename.empty())
        return buildError(400, "Empty filename", server);
    std::string filepath = location.upload_store + "/" + filename;
    int fd = open(filepath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return buildError(500, "Failed to open file for writing", server);
    size_t written = 0;
    while (written < contentSize) {
        ssize_t n = write(fd, contentPtr + written, contentSize - written);
        if (n < 0) {
            close(fd);
            return buildError(500, "Write error", server);
        }
        written += static_cast<size_t>(n);
    }
    close(fd);
    return makeUploadResponse(201, "File uploaded", filename);
}
