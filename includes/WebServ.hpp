#pragma once
#include <iostream>
#include "ServerConfig.hpp"
#include <vector>
#include <string>
#include <map>
#include <list>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include "HttpParser.hpp"
#include <algorithm>
#include "LocationConfig.hpp"
#include "Responder.hpp"
#include <ctime>
#include <sys/epoll.h>

#define TIMEOUT_SECONDS 30

class WebServ
{
public:
    WebServ(const std::vector<ServerConfig> &configs);
    ~WebServ();

    void start();

private:
    std::vector<ServerConfig> _servers;
    std::vector<int> _listenSockets;
    std::map<int, HttpParser> _parsers;
    std::map<int, size_t> _listenSockettoServerIndex;
    std::map<int, size_t> _clientToServerIndex;
    std::map<int, std::string> _writeBuffers;
    std::map<int, time_t> _lastActivity;
    std::map< std::pair<std::string,int>, std::vector<ServerConfig> > serverGroups;
    std::map<int, std::vector<ServerConfig> > _serversForSocket;
    std::map<int, const std::vector<ServerConfig>*> _clientToServers;
    int _epoll_fd;

    void initSockets();
    void mainLoop();
    void acceptNewConnection(int listen_fd);
    void handleClientRead(int fd, Responder &responder);
    void handleClientWrite(int fd);
    void closeClient(int fd);
    void checkTimeouts();
    const ServerConfig &chooseServer(const std::vector<ServerConfig> &serversVec, const std::string &hostName);
};

std::string extractHostWithoutPort(const std::string &host);