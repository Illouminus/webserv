#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"


class Responder {
    private:
    

    public:
        Responder(ServerConfig const &config, const std::string &path);
        ~Responder();
        const LocationConfig *findLocation(ServerConfig const &conf, const std::string &path);

};