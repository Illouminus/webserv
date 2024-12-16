#include "ServerConfig.hpp"

ServerConfig::ServerConfig() : host("0.0.0.0"), port(80), max_body_size(0), autoindex(false) {}
ServerConfig::ServerConfig(const ServerConfig &other)
{
	*this = other;
}
ServerConfig &ServerConfig::operator=(const ServerConfig &other)
{
	if (this != &other)
	{
		host = other.host;
		port = other.port;
		server_name = other.server_name;
		root = other.root;
		max_body_size = other.max_body_size;
		autoindex = other.autoindex;
		error_pages = other.error_pages;
		methods = other.methods;
		locations = other.locations;
	}
	return *this;
}
ServerConfig::~ServerConfig() {}

void ServerConfig::reset()
{
	host.clear();
	port = 80;
	server_name.clear();
	root.clear();
	max_body_size = 0;
	autoindex = false;
	error_pages.clear();
	methods.clear();
	locations.clear();
}
