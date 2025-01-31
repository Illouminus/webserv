#include "WebServ.hpp"
#include "HttpParser.hpp"
#include "HttpResponse.hpp"
#include "Responder.hpp"
#include <sys/time.h>
#include <cerrno>
#include <cstring>

#define MAX_EVENTS 1000
#define EPOLL_TIMEOUT 1000

std::string extractHostWithoutPort(const std::string &host)
{
    size_t colonPos = host.find(':');
    if (colonPos != std::string::npos)
    {
        return host.substr(0, colonPos);
    }
    return host;
}

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
	mainLoop();
}

const ServerConfig &WebServ::chooseServer(const std::vector<ServerConfig> &serversVec, 
                                          const std::string &hostName)
{
    for (size_t i = 0; i < serversVec.size(); i++)
    {
        if (serversVec[i].server_name == hostName)
            return serversVec[i];
    }
    // Если не нашли — возвращаем первый (дефолтный)
    return serversVec[0];
}


WebServ::WebServ(const std::vector<ServerConfig> &configs)
{
    for (size_t i = 0; i < configs.size(); i++)
    {
        std::string host = configs[i].getHost();
        int port         = configs[i].getPort();
        std::pair<std::string,int> key = std::make_pair(host, port);
        serverGroups[key].push_back(configs[i]);
    }
    initSockets();
}


void WebServ::initSockets()
{
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1)
		throw std::runtime_error("epoll_create1 failed");

	for (std::map< std::pair<std::string,int>, std::vector<ServerConfig> >::iterator 
            it = serverGroups.begin(); it != serverGroups.end(); ++it)
	{

		std::string host = it->first.first;
		int port = it->first.second;


		int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (listenSocket == -1)
			throw std::runtime_error("Error creating socket");

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
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = inet_addr(host.c_str());

		if (bind(listenSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		{
			close(listenSocket);
			throw std::runtime_error("Error binding socket");
		}

		if (listen(listenSocket, 32) < 0)
		{
			close(listenSocket);
			throw std::runtime_error("Error listening on socket");
		}


		struct epoll_event event;
		event.events = EPOLLIN;
		event.data.fd = listenSocket;
		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, listenSocket, &event) == -1)
		{
			close(listenSocket);
			throw std::runtime_error("epoll_ctl listen failed");
		}

		_listenSockets.push_back(listenSocket);
		_serversForSocket[listenSocket] = it->second;
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

		HttpParser &parser = _parsers[fd];
        parser.appendData(std::string(buffer, bytes_read));

		_parsers[fd].appendData(std::string(buffer, bytes_read));
		
        if (parser.headersComplete() && !parser.serverSelected())
        {
            std::string hostHeader = parser.getHeader("Host");
            std::string hostOnly = extractHostWithoutPort(hostHeader);

            const std::vector<ServerConfig> &sv = *(_clientToServers[fd]);
            const ServerConfig &chosen = chooseServer(sv, hostOnly);

            // устанавливаем max_body_size
			parser.setChosenServer(chosen);
            parser.setMaxBodySize(chosen.max_body_size);

            // отмечаем, что сервер уже выбран
            parser.setServerSelected(true);
        }


		 if (parser.hasError()) {
			const ServerConfig *srv = parser.serverIsChosen() ? parser.getChosenServer()
            : NULL;
            HttpResponse resp;
            if (parser.getErrorCode() == ERR_413) {
                resp = responder.makeErrorResponse(413, "Payload Too Large", *srv,
                                                   "Request Entity Too Large\n");
            } else {
                resp = responder.makeErrorResponse(400, "Bad Request", *srv,
                                                   "Bad Request\n");
            }

            _writeBuffers[fd] = resp.toString();
            struct epoll_event event;
            event.events = EPOLLOUT | EPOLLET;
            event.data.fd = fd;
            epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event);
            return;  
        }

		if (_parsers[fd].isComplete())
		{
			const ServerConfig *srv = parser.getChosenServer();
			HttpResponse resp = responder.handleRequest(_parsers[fd], *srv);
			_writeBuffers[fd] = resp.toString();

			// Меняем событие на запись
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

	_parsers[client_fd] = HttpParser();
    _lastActivity[client_fd] = time(NULL);
	_clientToServers[client_fd] = &(_serversForSocket[listen_fd]);
}

void WebServ::checkTimeouts()
{
    time_t now = time(NULL);

    std::map<int, time_t>::iterator it = _lastActivity.begin();
    while (it != _lastActivity.end())
    {
        int fd = it->first;
        time_t last = it->second;

        if (now - last > TIMEOUT_SECONDS)
        {
            closeClient(fd);
            it = _lastActivity.begin();
        }
        else
        {
            ++it;
        }
    }
}
