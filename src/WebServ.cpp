#include "WebServ.hpp"
#include "HttpParser.hpp"
#include "HttpResponse.hpp"
#include "Responder.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <algorithm>

// ====================
//    Конструкторы
// ====================
WebServ::~WebServ() {}

WebServ::WebServ(const std::vector<ServerConfig> &configs)
	 : _servers(configs)
{
	this->initSockets();
}

WebServ::WebServ(const WebServ &other)
	 : _servers(other._servers) {}

WebServ &WebServ::operator=(const WebServ &other)
{
	if (this != &other)
	{
		_servers = other._servers;
	}
	return *this;
}

// ====================
//    Основной start
// ====================
void WebServ::start()
{
	this->mainLoop();
}

// ====================
//    initSockets
// ====================
void WebServ::initSockets()
{
	for (size_t i = 0; i < _servers.size(); i++)
	{
		int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (listenSocket == -1)
		{
			std::cerr << "Error creating socket\n";
			exit(EXIT_FAILURE);
		}

		int opt = 1;
		if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		{
			std::cerr << "Error setting socket options\n";
			exit(EXIT_FAILURE);
		}

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(_servers[i].getPort());
		addr.sin_addr.s_addr = inet_addr(_servers[i].getHost().c_str());

		if (bind(listenSocket, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		{
			std::cerr << "Error binding socket\n";
			exit(EXIT_FAILURE);
		}

		if (listen(listenSocket, 100) == -1)
		{
			std::cerr << "Error listening on socket\n";
			exit(EXIT_FAILURE);
		}

		// Сохраняем
		_listenSockets.push_back(listenSocket);
		_listenSockettoServerIndex[listenSocket] = i;
	}
}

// ====================
//    getListenSockets
// ====================
void WebServ::getListenSockets()
{
	for (size_t i = 0; i < _listenSockets.size(); i++)
	{
		std::cout << _listenSockets[i] << "\n";
	}
}

// ====================
//    getMaxFd
// ====================
int WebServ::getMaxFd()
{
	int max_fd = -1;

	if (!_listenSockets.empty())
	{
		int max_listen_fd = *std::max_element(_listenSockets.begin(), _listenSockets.end());
		if (max_listen_fd > max_fd)
			max_fd = max_listen_fd;
	}

	if (!_clientSockets.empty())
	{
		int max_client_fd = *std::max_element(_clientSockets.begin(), _clientSockets.end());
		if (max_client_fd > max_fd)
			max_fd = max_client_fd;
	}
	return max_fd;
}

// ====================
//    mainLoop
// ====================
void WebServ::mainLoop()
{
	Responder responder;
	while (true)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		for (size_t i = 0; i < _listenSockets.size(); i++)
			FD_SET(_listenSockets[i], &readfds);

		for (std::list<int>::iterator it = _clientSockets.begin(); it != _clientSockets.end(); ++it)
			FD_SET(*it, &readfds);

		int max_fd = getMaxFd();
		if (max_fd == -1)
			continue;

		if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			std::cerr << "Error in select\n";
			exit(EXIT_FAILURE);
		}
		acceptNewConnections(readfds);
		handleClientSockets(readfds, responder);
	}
}

// ====================
//   acceptNewConnections
// ====================
void WebServ::acceptNewConnections(fd_set &readfds)
{
	for (size_t i = 0; i < _listenSockets.size(); i++)
	{
		int listenFd = _listenSockets[i];
		if (FD_ISSET(listenFd, &readfds))
		{
			struct sockaddr_in addr;
			socklen_t addr_len = sizeof(addr);
			int clientSocket = accept(listenFd, (struct sockaddr *)&addr, &addr_len);
			if (clientSocket > 0)
			{
				size_t serverIndex = _listenSockettoServerIndex[listenFd];
				fcntl(clientSocket, F_SETFL, O_NONBLOCK);
				_clientSockets.push_back(clientSocket);
				_parsers[clientSocket] = HttpParser();
				_clientToServerIndex[clientSocket] = serverIndex;
			}
			else
			{
				std::cerr << "Error accepting connection\n";
				exit(EXIT_FAILURE);
			}
		}
	}
}

// ====================
//   handleClientSockets
// ====================
void WebServ::handleClientSockets(fd_set &readfds, Responder &responder)
{
	for (std::list<int>::iterator it = _clientSockets.begin();
		  it != _clientSockets.end();)
	{
		int fd = *it;
		if (FD_ISSET(fd, &readfds))
		{
			char buf[1024];
			int bytes = recv(fd, buf, sizeof(buf), 0);
			if (bytes <= 0)
			{
				close(fd);
				_parsers.erase(fd);
				_clientToServerIndex.erase(fd);
				it = _clientSockets.erase(it);
				continue;
			}
			else
			{
				size_t serverIndex = _clientToServerIndex[fd];
				ServerConfig &server = _servers[serverIndex];
				_parsers[fd].appendData(std::string(buf, bytes), server.max_body_size);
				if (_parsers[fd].hasError())
				{
					HttpResponse resp;
					resp.setStatus(413, "Payload Too Large");
					resp.setHeader("Content-Type", "text/plain");
					resp.setBody("Request Entity Too Large\n");

					// (Опционально) подгружаем custom error page, если есть:
					// responder.handleErrorPage(resp, server, 413);
					std::string rawResponse = resp.toString();
					send(fd, rawResponse.c_str(), rawResponse.size(), 0);
					close(fd);
					_parsers.erase(fd);
					_clientToServerIndex.erase(fd);
					it = _clientSockets.erase(it);
					continue;
				}

				if (_parsers[fd].isComplete())
				{
					HttpResponse resp = responder.handleRequest(_parsers[fd], server);
					std::string rawResponse = resp.toString();
					std::cout << rawResponse << "\n";
					send(fd, rawResponse.c_str(), rawResponse.size(), 0);
					if (!_parsers[fd].isKeepAlive())
					{
						close(fd);
						_parsers.erase(fd);
						_clientToServerIndex.erase(fd);
						it = _clientSockets.erase(it);
						continue;
					}
					else
					{
						_parsers[fd] = HttpParser();
					}
				}
			}
		}
		++it;
	}
}