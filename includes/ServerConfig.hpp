#pragma once

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

class ServerConfig
{
public:
	ServerConfig();
	ServerConfig(const ServerConfig &other);
	ServerConfig &operator=(const ServerConfig &other);
	~ServerConfig();

	std::string host;
	int port;
	std::string server_name;
	std::string root;
	size_t max_body_size;
	bool autoindex;
	std::map<int, std::string> error_pages;
	std::vector<std::string> methods;
	std::vector<LocationConfig> locations;

	void reset();

	int getPort() const;

	std::string getHost() const;
};
