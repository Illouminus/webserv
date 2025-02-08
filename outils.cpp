#include "ServerConfig.hpp"
#include <iostream>
#include <vector>

void printConf(const std::vector<ServerConfig> &servers)
{
	for (size_t i = 0; i < servers.size(); i++)
	{
		const ServerConfig &srv = servers[i];
		std::cout << "Server " << i << ": " << srv.host << ":" << srv.port << "\n";
		std::cout << "  server_name: " << srv.server_name << "\n";
		std::cout << "  root: " << srv.root << "\n";
		std::cout << "  max_body_size: " << srv.max_body_size << "\n";
		std::cout << "  autoindex: " << (srv.autoindex ? "on" : "off") << "\n";

		std::cout << "  error_pages:\n";
		for (std::map<int, std::string>::const_iterator it = srv.error_pages.begin(); it != srv.error_pages.end(); ++it)
		{
			std::cout << "    " << it->first << " -> " << it->second << "\n";
		}

		std::cout << "  methods:";
		for (size_t j = 0; j < srv.methods.size(); j++)
		{
			std::cout << " " << srv.methods[j];
		}
		std::cout << "\n";

		// Locations
		for (size_t k = 0; k < srv.locations.size(); k++)
		{
			const LocationConfig &loc = srv.locations[k];
			std::cout << "  Location: " << loc.path << "\n";
			std::cout << "    methods:";
			for (size_t j = 0; j < loc.methods.size(); j++)
			{
				std::cout << " " << loc.methods[j];
			}
			std::cout << "\n";
			std::cout << "    root: " << loc.root << "\n";
			std::cout << "    index: " << loc.index << "\n";
			std::cout << "    autoindex: " << (loc.autoindex ? "on" : "off") << "\n";
			std::cout << "    cgi_pass: " << loc.cgi_pass << "\n";
			std::cout << "    cgi_extension: " << loc.cgi_extension << "\n";
		}
	}
}