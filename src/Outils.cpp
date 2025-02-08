#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

std::map<std::string, std::string> parseCookieString(const std::string &cookieStr)
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



std::string generateRandomSessionID()
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


std::string extractExtention(std::string path)
{
    size_t pos = path.rfind('.');
    if (pos == std::string::npos)
        return "";
    return path.substr(pos);
}