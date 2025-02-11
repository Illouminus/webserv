#include "Outils.hpp"
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


Outils::Outils() {};
Outils::~Outils() {};

std::map<std::string, std::string> Outils::parseCookieString(const std::string &cookieStr)
{
    std::map<std::string, std::string> result;
    if (cookieStr.empty())
        return result;

    // Разбиваем по ";"
    // (Если в реальности приходит "; " — оно тоже сработает, потому что getline разделит по ';')
    std::stringstream ss(cookieStr);
    std::string token;
    while (std::getline(ss, token, ';'))
    {
        // trim leading/trailing spaces (C++98-совместимо)
        // trim left
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token[0])))
            token.erase(0, 1);
        // trim right
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token[token.size() - 1])))
            token.erase(token.size() - 1, 1);

        // token что-то вроде "key=value"
        size_t eqPos = token.find('=');
        if (eqPos != std::string::npos)
        {
            std::string key = token.substr(0, eqPos);
            std::string val = token.substr(eqPos + 1);

            // trim left key
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key[0])))
                key.erase(0, 1);
            // trim right key
            while (!key.empty() && std::isspace(static_cast<unsigned char>(key[key.size() - 1])))
                key.erase(key.size() - 1, 1);

            // trim left val
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val[0])))
                val.erase(0, 1);
            // trim right val
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val[val.size() - 1])))
                val.erase(val.size() - 1, 1);

            result[key] = val;
        }
    }

    return result;
}



std::string Outils::generateRandomSessionID()
{
    // Very trivial random
    // You can do something better with <random> or time
    char buf[16];
    for (int i = 0; i < 8; i++)
    {
        int r = rand() % 36;
        if (r < 10) buf[i] = '0' + r;
        else buf[i] = 'a' + (r - 10);
    }
    buf[8] = '\0';
    return std::string(buf);
}


std::string Outils::extractExtention(std::string path)
{
    size_t pos = path.rfind('.');
    if (pos == std::string::npos)
        return "";
    return path.substr(pos);
}


std::string Outils::trim(const std::string &s) {
    std::string result = s;
    // Удаляем пробелы с начала
    size_t start = 0;
    while (start < result.size() && std::isspace(result[start])) {
        ++start;
    }
    // Удаляем пробелы с конца
    size_t end = result.size();
    while (end > start && std::isspace(result[end - 1])) {
        --end;
    }
    return result.substr(start, end - start);
}


void Outils::printConf(const std::vector<ServerConfig> &servers)
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
			std::cout << "    max_body_size: " << loc.max_body_size << "\n";
		}
	}
}