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
	WebServ(const WebServ &other);
	WebServ &operator=(const WebServ &other);
	~WebServ();

	void start();
	void getListenSockets();

private:
	std::vector<ServerConfig> _servers;
	std::vector<int> _listenSockets;
	std::list<int> _clientSockets;
	std::map<int, HttpParser> _parsers;
	std::map<int, size_t> _listenSockettoServerIndex;
	std::map<int, size_t> _clientToServerIndex;
	std::map<int, std::string> _writeBuffers;
	std::map<int, time_t> _lastActivity;
	int _max_fd;
	int _epoll_fd;

	void initSockets();
	void mainLoop();
	int getMaxFd();
	void updateMaxFd(int fd);
	void acceptNewConnection(int listen_fd);
	void handleClientRead(int fd, Responder &responder);
	void handleClientWrite(int fd);
	void closeConnection(int fd, std::list<int>::iterator &it);
	void closeClient(int fd);
	void checkTimeouts();
};