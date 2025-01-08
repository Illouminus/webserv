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

	// Проверяем слушающие
	if (!_listenSockets.empty())
	{
		int max_listen_fd = *std::max_element(_listenSockets.begin(), _listenSockets.end());
		if (max_listen_fd > max_fd)
			max_fd = max_listen_fd;
	}
	// Проверяем клиентские
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
	// Создадим responder один раз, можно и на каждый запрос, но обычно хватает одного
	Responder responder;

	while (true)
	{
		fd_set readfds;
		FD_ZERO(&readfds);

		// Добавляем все слушающие сокеты
		for (size_t i = 0; i < _listenSockets.size(); i++)
		{
			FD_SET(_listenSockets[i], &readfds);
		}
		// Добавляем уже подключенных клиентов
		for (std::list<int>::iterator it = _clientSockets.begin();
			  it != _clientSockets.end(); ++it)
		{
			FD_SET(*it, &readfds);
		}

		int max_fd = getMaxFd();
		if (max_fd == -1)
			continue; // нет ни одного сокета

		// Ждем события
		if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			std::cerr << "Error in select\n";
			exit(EXIT_FAILURE);
		}

		// 1) Принимаем новые соединения
		acceptNewConnections(readfds);

		// 2) Обрабатываем уже подключенных клиентов
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
				// определяем, к какому серверу относится
				size_t serverIndex = _listenSockettoServerIndex[listenFd];

				// настроим неблокирующий
				fcntl(clientSocket, F_SETFL, O_NONBLOCK);

				// сохраняем
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

		// Проверяем, готов ли fd к чтению
		if (FD_ISSET(fd, &readfds))
		{
			char buf[1024];
			int bytes = recv(fd, buf, sizeof(buf), 0);
			if (bytes <= 0)
			{
				// -1 ошибка или 0 клиент закрыл
				close(fd);
				_parsers.erase(fd);
				_clientToServerIndex.erase(fd);
				it = _clientSockets.erase(it);
				continue;
			}
			else
			{
				// Добавляем прочитанные данные в парсер
				_parsers[fd].appendData(std::string(buf, bytes));

				// Если парсер собрал весь запрос
				if (_parsers[fd].isComplete())
				{
					// Узнаём, к какому серверу принадлежит этот сокет
					size_t serverIndex = _clientToServerIndex[fd];
					ServerConfig &server = _servers[serverIndex];

					// Вызываем Responder
					HttpResponse resp = responder.handleRequest(_parsers[fd], server);

					// Отправляем
					std::string rawResponse = resp.toString();
					std::cout << rawResponse << "\n";
					send(fd, rawResponse.c_str(), rawResponse.size(), 0);

					// Проверяем, keep-alive или закрыть
					if (!_parsers[fd].isKeepAlive())
					{
						close(fd);
						_parsers.erase(fd);
						_clientToServerIndex.erase(fd);
						it = _clientSockets.erase(it);
						continue; // переходим к следующему
					}
					else
					{
						// Перезапускаем парсер, чтобы прочитать следующий запрос
						_parsers[fd] = HttpParser();
					}
				}
			}
		}
		++it; // если не удалили итератор, двигаемся дальше
	}
}