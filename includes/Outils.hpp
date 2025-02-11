#pragma once
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"


class Outils
{
    public:
        Outils();
        ~Outils();
        std::map<std::string, std::string> parseCookieString(const std::string &cookieStr);
        std::string generateRandomSessionID();
        std::string extractExtention(std::string path);
        std::string trim(const std::string &s);
        void printConf(const std::vector<ServerConfig> &servers);
};