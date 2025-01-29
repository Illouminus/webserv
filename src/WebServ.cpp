#include "WebServ.hpp"
#include "HttpParser.hpp"
#include "HttpResponse.hpp"
#include "Responder.hpp"
#include <sys/time.h>
#include <cerrno>
#include <cstring>

#define MAX_EVENTS 1000
#define EPOLL_TIMEOUT 1000

WebServ::~WebServ()
{
	for (std::vector<int>::iterator it = _listenSockets.begin(); it != _listenSockets.end(); ++it)
	{
		close(*it);
	}
	close(_epoll_fd);
}

void WebServ::start()
{
	this->mainLoop();
}

WebServ::WebServ(const std::vector<ServerConfig> &configs)
	 : _servers(configs), _max_fd(-1)
{
	this->initSockets();
}

void WebServ::initSockets()
{
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1)
		throw std::runtime_error("epoll_create1 failed");

	for (size_t i = 0; i < _servers.size(); i++)
	{
		int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (listenSocket == -1)
		{
			throw std::runtime_error("Error creating socket");
		}

		int opt = 1;
		if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		{
			close(listenSocket);
			throw std::runtime_error("Error setting socket options");
		}

		fcntl(listenSocket, F_SETFL, O_NONBLOCK);

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(_servers[i].getPort());
		addr.sin_addr.s_addr = inet_addr(_servers[i].getHost().c_str());

		if (bind(listenSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		{
			close(listenSocket);
			throw std::runtime_error("Error binding socket");
		}

		if (listen(listenSocket, 100) < 0)
		{
			close(listenSocket);
			throw std::runtime_error("Error listening on socket");
		}

		_listenSockets.push_back(listenSocket);
		_listenSockettoServerIndex[listenSocket] = i;
		updateMaxFd(listenSocket);

		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = listenSocket;
		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, listenSocket, &event) == -1)
		{
			close(listenSocket);
			throw std::runtime_error("epoll_ctl listen failed");
		}
	}
}

void WebServ::mainLoop()
{
	Responder responder;
	struct epoll_event events[MAX_EVENTS];

	while (true)
	{
		int num_events = epoll_wait(_epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT);
		if (num_events == -1 && errno != EINTR)
		{
			std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
			continue;
		}

		checkTimeouts();

		for (int i = 0; i < num_events; ++i)
		{
			int fd = events[i].data.fd;
			uint32_t event_mask = events[i].events;

			// Обработка новых подключений
			if (std::find(_listenSockets.begin(), _listenSockets.end(), fd) != _listenSockets.end())
			{
				acceptNewConnection(fd);
			}
			// Обработка клиентских событий
			else
			{
				if (event_mask & EPOLLIN)
				{
					handleClientRead(fd, responder);
				}
				if (event_mask & EPOLLOUT)
				{
					handleClientWrite(fd);
				}
				if (event_mask & (EPOLLERR | EPOLLHUP))
				{
					closeClient(fd);
				}
			}
		}
	}
}

void WebServ::handleClientRead(int fd, Responder &responder)
{
	char buffer[4096];
	ssize_t bytes_read;

	while ((bytes_read = recv(fd, buffer, sizeof(buffer), 0)) > 0)
	{
		_lastActivity[fd] = time(NULL);
		ServerConfig &server = _servers[_clientToServerIndex[fd]];

		try
		{
			_parsers[fd].appendData(std::string(buffer, bytes_read), server.max_body_size);
		}
		catch (const std::exception &e)
		{
		}


		if (_parsers[fd].isComplete())
		{
			HttpResponse resp = responder.handleRequest(_parsers[fd], server);
			_writeBuffers[fd] = resp.toString();

			// Меняем событие на запись
			struct epoll_event event;
			event.events = EPOLLOUT | EPOLLET;
			event.data.fd = fd;
			epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
		}
		else if(_parsers[fd].hasError())
		{
			HttpResponse resp;

			if(_parsers[fd].getErrorCode() == ERR_413)
				resp = responder.makeErrorResponse(413, "Payload Too Large", server, "Request Entity Too Large\n");
			else
				resp = responder.makeErrorResponse(400, "Bad Request", server, "Bad Request\n");
				
			_writeBuffers[fd] = resp.toString();

			struct epoll_event event;
			event.events = EPOLLOUT | EPOLLET;
			event.data.fd = fd;
			epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
		}
	}

	if (bytes_read == 0 || (bytes_read == -1 && errno != EAGAIN))
	{
		closeClient(fd);
	}
}

void WebServ::handleClientWrite(int fd)
{
	std::string &buffer = _writeBuffers[fd];
	ssize_t sent = send(fd, buffer.c_str(), buffer.size(), MSG_NOSIGNAL);

	if (sent > 0)
	{
		buffer.erase(0, sent);
		_lastActivity[fd] = time(NULL);
	}

	if (buffer.empty())
	{
		// Возвращаем обратно на чтение
		_parsers[fd] = HttpParser();
		struct epoll_event event;
		event.events = EPOLLIN | EPOLLET;
		event.data.fd = fd;
		epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);

		if (!_parsers[fd].isKeepAlive())
		{
			closeClient(fd);
		}
	}
	else if (sent == -1 && errno != EAGAIN)
	{
		closeClient(fd);
	}
}

void WebServ::closeClient(int fd)
{
	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	_parsers.erase(fd);
	_clientToServerIndex.erase(fd);
	_writeBuffers.erase(fd);
	_lastActivity.erase(fd);
}


void WebServ::acceptNewConnection(int listen_fd)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_fd == -1)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			std::cerr << "accept error: " << strerror(errno) << std::endl;
		}
		return;
	}

	fcntl(client_fd, F_SETFL, O_NONBLOCK);

	// Регистрируем клиентский сокет в epoll
	struct epoll_event event;
	event.events = EPOLLIN | EPOLLET; // Edge-triggered режим
	event.data.fd = client_fd;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1)
	{
		close(client_fd);
		std::cerr << "epoll_ctl client failed" << std::endl;
		return;
	}

	// Сохраняем состояние клиента
	this->_clientToServerIndex[client_fd] = _listenSockettoServerIndex[listen_fd];
	_parsers[client_fd] = HttpParser();
	_lastActivity[client_fd] = time(NULL);
}

void WebServ::checkTimeouts()
{
	time_t now = time(NULL);
	for (std::list<int>::iterator it = _clientSockets.begin(); it != _clientSockets.end();)
	{
		int fd = *it;
		if (now - _lastActivity[fd] > TIMEOUT_SECONDS)
		{
			std::cout << "Timeout closing fd " << fd << std::endl;
			closeConnection(fd, it);
		}
		else
		{
			++it;
		}
	}
}

void WebServ::closeConnection(int fd, std::list<int>::iterator &it)
{
	close(fd);
	_parsers.erase(fd);
	_clientToServerIndex.erase(fd);
	_writeBuffers.erase(fd);
	_lastActivity.erase(fd);
	it = _clientSockets.erase(it);

	// Recalculate max fd
	_max_fd = getMaxFd();
}

int WebServ::getMaxFd()
{
	int max = -1;
	for (std::vector<int>::const_iterator it = _listenSockets.begin(); it != _listenSockets.end(); ++it)
	{
		if (*it > max)
			max = *it;
	}
	for (std::list<int>::const_iterator it = _clientSockets.begin(); it != _clientSockets.end(); ++it)
	{
		if (*it > max)
			max = *it;
	}
	return max;
}

void WebServ::updateMaxFd(int fd)
{
	if (fd > _max_fd)
	{
		_max_fd = fd;
	}
}

// Other methods (operator=, copy constructor etc.) remain similar