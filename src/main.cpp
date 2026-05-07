#include "Parser.hpp"
#include "PollServer.hpp"
#include <iostream>
#include <signal.h>

int main(int ac, char **av){
    signal(SIGPIPE, SIG_IGN);

    if (ac != 2){
        std::cerr << "Usage: ./webserv <config.conf>" << std::endl;
        return 1;
    }
    try {
        ConfigParser parser(ac, av);
        parser.parse();
        const std::vector<ServerConfig> &servers = parser.getServers();
        if (servers.empty()) {
            std::cerr << "Error: no server blocks found in config file" << std::endl;
            return 1;
        }
        PollServer pollServer;
        for (size_t i = 0; i < servers.size(); ++i) {
            const ServerConfig &conf = servers[i];
            pollServer.addServer(conf);
            std::cout << "Loaded server: " << conf.server_name
                      << " listening on " << conf.host
                      << ":" << conf.listen << std::endl;
        }
        pollServer.runServer(servers);
    }
    catch (std::exception &e){
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
