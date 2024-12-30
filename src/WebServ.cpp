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