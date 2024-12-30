#include "WebServ.hpp"

WebServ::~WebServ() {}
WebServ::WebServ(const std::vector<ServerConfig> &configs) : _servers(configs)
{
	this->initSockets();
}
WebServ::WebServ(const WebServ &other) : _servers(other._servers) {}
WebServ &WebServ::operator=(const WebServ &other)
{
	if (this != &other)
	{
		_servers = other._servers;
	}
	return *this;
}

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
		_listenSockets.push_back(listenSocket);
	}
}

void WebServ::getListenSockets()
{
	for (size_t i = 0; i < _listenSockets.size(); i++)
	{
		std::cout << _listenSockets[i] << "\n";
	}
}

void WebServ::start()
{
	this->mainLoop();
}

int WebServ::getMaxFd()
{
	int max_fd = -1;

	// Если вектор слушающих сокетов не пуст, найдём максимум
	if (!_listenSockets.empty())
	{
		int max_listen_fd = *std::max_element(_listenSockets.begin(), _listenSockets.end());
		if (max_listen_fd > max_fd)
			max_fd = max_listen_fd;
	}

	// Если список клиентских сокетов не пуст, найдём максимум
	if (!_clientSockets.empty())
	{
		// std::list<int> тоже поддерживает итераторы для std::max_element
		int max_client_fd = *std::max_element(_clientSockets.begin(), _clientSockets.end());
		if (max_client_fd > max_fd)
			max_fd = max_client_fd;
	}

	return max_fd;
}

void WebServ::mainLoop()
{
	while (1)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		for (size_t i = 0; i < _listenSockets.size(); i++)
		{
			FD_SET(_listenSockets[i], &readfds);
		}
		for (std::list<int>::iterator it = _clientSockets.begin(); it != _clientSockets.end(); it++)
		{
			FD_SET(*it, &readfds);
		}
		int max = this->getMaxFd();
		std::cout << "Max fd: " << max << "\n";
		if (max == -1)
			continue;

		if (select(max + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			std::cerr << "Error in select\n";
			exit(EXIT_FAILURE);
		}

		for (size_t i = 0; i < _listenSockets.size(); i++)
		{
			if (FD_ISSET(_listenSockets[i], &readfds))
			{
				struct sockaddr_in addr;
				socklen_t addr_len = sizeof(addr);
				int clientSocket = accept(_listenSockets[i], (struct sockaddr *)&addr, &addr_len);
				if (clientSocket > 0)
				{
					fcntl(clientSocket, F_SETFL, O_NONBLOCK);
					_clientSockets.push_back(clientSocket);
				}
				else
				{
					std::cerr << "Error accepting connection\n";
					exit(EXIT_FAILURE);
				}
			}
		}
	}
}