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


struct SessionData
{
    std::string ip;
    std::string userAgent;
    time_t lastVisit;
};

class Outils
{
    public:
        Outils();
        ~Outils();
        std::map<std::string, std::string> parseCookieString(const std::string &cookieStr);
        std::map<std::string, SessionData> g_sessions;
        std::string generateRandomSessionID();
        std::string extractExtention(std::string path);
        std::string trim(const std::string &s);
        void printConf(const std::vector<ServerConfig> &servers);
        void storeSessionData(const std::string &sid, const std::string &ip, const std::string &userAgent);
        SessionData *getSessionData(const std::string &sid);
};