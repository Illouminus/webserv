#pragma once

#include "ServerConfig.hpp"
#include <string>
#include <vector>

class Parser
{
public:
	Parser();
	Parser(const Parser &other);
	Parser &operator=(const Parser &other);
	~Parser();

	void parseConfig(const std::string &filename);
	const std::vector<ServerConfig> &getServers() const;

private:
	std::vector<ServerConfig> servers;

	// Лексер:
	std::vector<std::string> tokens;
	size_t currentIndex;

	void tokenize(const std::string &content);

	bool isEnd();
	std::string peekToken();
	std::string getToken();
	void expectToken(const std::string &expected);

	void parseServers();
	void parseServerBlock(ServerConfig &srv);
	void parseLocationBlock(ServerConfig &srv);
	void parseServerDirective(ServerConfig &srv, const std::string &directive);
	void parseLocationDirective(LocationConfig &loc, const std::string &directive);

	// Вспомогательные парсеры:
	void parseListen(ServerConfig &srv, const std::string &value);
	size_t parseSize(const std::string &value);
	int parseStatusCode(const std::string &value);
};
