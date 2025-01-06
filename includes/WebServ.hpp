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
#include "HttpParser.hpp"

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

	void initSockets();
	void mainLoop();
	int getMaxFd();
};