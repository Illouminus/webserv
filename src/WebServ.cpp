/**
 * WebServ.cpp
 *
 * Implementation of the WebServ class. This class orchestrates:
 *  - Creation of listening sockets
 *  - The main event loop using select()
 *  - Accepting new connections
 *  - Handling existing client sockets (reading data into parsers, passing to Responder, etc.)
 */

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
//    Constructors
// ====================

WebServ::~WebServ()
{
	// Cleanup if needed (closing sockets, etc.)
}

WebServ::WebServ(const std::vector<ServerConfig> &configs)
	 : _servers(configs)
{
	// Initialize all listening sockets for each configured server
	this->initSockets();
}

WebServ::WebServ(const WebServ &other)
	 : _servers(other._servers)
{
	// We do not copy listening sockets or client info here
	// because each WebServ instance has its own runtime state.
}

WebServ &WebServ::operator=(const WebServ &other)
{
	if (this != &other)
	{
		_servers = other._servers;
		// Listening sockets & clients are not duplicated
	}
	return *this;
}

// ====================
//    Start / Main Loop
// ====================

/**
 * start()
 *
 * Begins the main event loop. This function calls mainLoop()
 * which runs indefinitely, listening for new connections
 * and handling existing clients.
 */
void WebServ::start()
{
	this->mainLoop();
}

/**
 * mainLoop()
 *
 * The core "event loop" that uses select() to:
 *   1) Monitor listening sockets for new connections
 *   2) Monitor client sockets for incoming data
 */
void WebServ::mainLoop()
{
	Responder responder; // The responder object to handle HTTP requests & build responses

	while (true)
	{
		fd_set readfds;
		FD_ZERO(&readfds);

		// Add listening sockets to read set
		for (size_t i = 0; i < _listenSockets.size(); i++)
		{
			FD_SET(_listenSockets[i], &readfds);
		}

		// Add existing client sockets to read set
		for (std::list<int>::iterator it = _clientSockets.begin();
			  it != _clientSockets.end(); ++it)
		{
			FD_SET(*it, &readfds);
		}

		// Determine the highest file descriptor
		int max_fd = getMaxFd();
		if (max_fd == -1)
		{
			// No sockets yet, skip
			continue;
		}

		// Wait for activity on any socket (no timeout)
		if (select(max_fd + 1, &readfds, NULL, NULL, NULL) == -1)
		{
			std::cerr << "Error in select\n";
			exit(EXIT_FAILURE);
		}

		// Handle new incoming connections
		acceptNewConnections(readfds);

		// Handle I/O on existing client sockets
		handleClientSockets(readfds, responder);
	}
}

// ====================
//    Socket Init
// ====================

/**
 * initSockets()
 *
 * For each ServerConfig, create a listening socket (TCP stream),
 * bind to the specified host:port, and start listening.
 */
void WebServ::initSockets()
{
	for (size_t i = 0; i < _servers.size(); i++)
	{
		// Create a TCP socket
		int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (listenSocket == -1)
		{
			std::cerr << "Error creating socket\n";
			exit(EXIT_FAILURE);
		}

		// Set SO_REUSEADDR
		int opt = 1;
		if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		{
			std::cerr << "Error setting socket options\n";
			exit(EXIT_FAILURE);
		}

		// Bind to specified host:port
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(_servers[i].getPort());
		addr.sin_addr.s_addr = inet_addr(_servers[i].getHost().c_str());

		if (bind(listenSocket, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		{
			std::cerr << "Error binding socket\n";
			exit(EXIT_FAILURE);
		}

		// Start listening with a backlog of 100
		if (listen(listenSocket, 100) == -1)
		{
			std::cerr << "Error listening on socket\n";
			exit(EXIT_FAILURE);
		}

		// Save the socket and map it to the server index
		_listenSockets.push_back(listenSocket);
		_listenSockettoServerIndex[listenSocket] = i;
	}
}

/**
 * getListenSockets()
 *
 * Debug method to list all listening sockets (if needed).
 */
void WebServ::getListenSockets()
{
	for (size_t i = 0; i < _listenSockets.size(); i++)
	{
		std::cout << _listenSockets[i] << "\n";
	}
}

/**
 * getMaxFd()
 *
 * Returns the highest file descriptor among listening
 * and client sockets, used for select().
 */
int WebServ::getMaxFd()
{
	int max_fd = -1;

	// Find max among listen sockets
	if (!_listenSockets.empty())
	{
		int max_listen_fd = *std::max_element(_listenSockets.begin(), _listenSockets.end());
		if (max_listen_fd > max_fd)
			max_fd = max_listen_fd;
	}

	// Find max among client sockets
	if (!_clientSockets.empty())
	{
		int max_client_fd = *std::max_element(_clientSockets.begin(), _clientSockets.end());
		if (max_client_fd > max_fd)
			max_fd = max_client_fd;
	}
	return max_fd;
}

// ====================
//   Accept New Clients
// ====================

/**
 * acceptNewConnections()
 *
 * Checks each listening socket to see if there's an incoming
 * connection. If so, accept() it, set non-blocking,
 * and store it in _clientSockets & _parsers.
 */
void WebServ::acceptNewConnections(fd_set &readfds)
{
	for (size_t i = 0; i < _listenSockets.size(); i++)
	{
		int listenFd = _listenSockets[i];
		// If the listening socket is ready to read => new connection is pending
		if (FD_ISSET(listenFd, &readfds))
		{
			struct sockaddr_in addr;
			socklen_t addr_len = sizeof(addr);
			int clientSocket = accept(listenFd, (struct sockaddr *)&addr, &addr_len);
			if (clientSocket > 0)
			{
				// Retrieve which server config this socket belongs to
				size_t serverIndex = _listenSockettoServerIndex[listenFd];

				// Make client socket non-blocking
				fcntl(clientSocket, F_SETFL, O_NONBLOCK);

				// Track client socket
				_clientSockets.push_back(clientSocket);

				// Create a new HttpParser instance for this socket
				_parsers[clientSocket] = HttpParser();

				// Remember which server config is used for this client
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
//   Handle Clients
// ====================

/**
 * handleClientSockets()
 *
 * Iterates through all client sockets, checks if they're ready to read.
 * If there's incoming data:
 *  - read it
 *  - feed it to the HttpParser
 *  - if parser sees an error => respond with error and close
 *  - if parser sees complete => pass to Responder for handling
 *  - if keep-alive is off => close
 */
void WebServ::handleClientSockets(fd_set &readfds, Responder &responder)
{
	for (std::list<int>::iterator it = _clientSockets.begin();
		  it != _clientSockets.end();)
	{
		int fd = *it;

		// Check if this client socket has data
		if (FD_ISSET(fd, &readfds))
		{
			char buf[1024];
			int bytes = recv(fd, buf, sizeof(buf), 0);

			if (bytes <= 0)
			{
				// Client closed connection or error
				close(fd);
				_parsers.erase(fd);
				_clientToServerIndex.erase(fd);
				it = _clientSockets.erase(it);
				continue;
			}
			else
			{
				// We have some data from client
				size_t serverIndex = _clientToServerIndex[fd];
				ServerConfig &server = _servers[serverIndex];

				// Append data to parser
				_parsers[fd].appendData(std::string(buf, bytes), server.max_body_size);

				// Check for parsing errors (e.g. 400, 413, etc.)
				if (_parsers[fd].hasError())
				{
					HttpResponse resp;
					if (_parsers[fd].getErrorCode() == ERR_413)
					{
						resp = responder.makeErrorResponse(413, "Payload Too Large", server, "Request Entity Too Large\n");
					}
					else
					{
						resp = responder.makeErrorResponse(400, "Bad Request", server, "Bad Request\n");
					}

					// Send error response
					std::string rawResponse = resp.toString();
					send(fd, rawResponse.c_str(), rawResponse.size(), 0);

					// Close the socket
					close(fd);
					_parsers.erase(fd);
					_clientToServerIndex.erase(fd);
					it = _clientSockets.erase(it);
					continue;
				}

				// If request is fully parsed => pass to Responder
				if (_parsers[fd].isComplete())
				{
					HttpResponse resp = responder.handleRequest(_parsers[fd], server);
					std::string rawResponse = resp.toString();

					// Debug output of response
					std::cout << rawResponse << "\n";

					// Send the final response to the client
					send(fd, rawResponse.c_str(), rawResponse.size(), 0);

					// Check if keep-alive is off => close socket
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
						// If keep-alive => reset parser for next request
						_parsers[fd] = HttpParser();
					}
				}
			}
		}
		// Move to next client
		++it;
	}
}
