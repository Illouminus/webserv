#include "Parser.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cstdlib> // atoi

Parser::Parser() : currentIndex(0) {}
Parser::Parser(const Parser &other) { *this = other; }
Parser &Parser::operator=(const Parser &other)
{
	if (this != &other)
	{
		servers = other.servers;
		tokens = other.tokens;
		currentIndex = other.currentIndex;
	}
	return *this;
}
Parser::~Parser() {}

const std::vector<ServerConfig> &Parser::getServers() const
{
	return servers;
}

void Parser::parseConfig(const std::string &filename)
{
	servers.clear();
	tokens.clear();
	currentIndex = 0;

	std::ifstream ifs(filename.c_str());
	if (!ifs)
		throw std::runtime_error("Cannot open config file");
	std::string content((std::istreambuf_iterator<char>(ifs)),
							  std::istreambuf_iterator<char>());
	ifs.close();

	tokenize(content);
	parseServers();
}

void Parser::tokenize(const std::string &content)
{
	std::string delimiters = " \t\n\r;";
	std::string buffer;
	for (size_t i = 0; i < content.size(); i++)
	{
		char c = content[i];
		if (c == '{' || c == '}' || c == ';')
		{
			if (!buffer.empty())
			{
				tokens.push_back(buffer);
				buffer.clear();
			}
			std::string s(1, c);
			tokens.push_back(s);
		}
		else if (isspace((unsigned char)c))
		{
			if (!buffer.empty())
			{
				tokens.push_back(buffer);
				buffer.clear();
			}
		}
		else
		{
			buffer.push_back(c);
		}
	}
	if (!buffer.empty())
	{
		tokens.push_back(buffer);
	}
}

bool Parser::isEnd()
{
	return currentIndex >= tokens.size();
}

std::string Parser::peekToken()
{
	if (isEnd())
		return "";
	return tokens[currentIndex];
}

std::string Parser::getToken()
{
	if (isEnd())
		throw std::runtime_error("Unexpected end of tokens");
	return tokens[currentIndex++];
}

void Parser::expectToken(const std::string &expected)
{
	std::string tk = getToken();
	if (tk != expected)
	{
		throw std::runtime_error("Expected '" + expected + "', got '" + tk + "'");
	}
}

void Parser::parseServers()
{
	while (!isEnd())
	{
		std::string t = peekToken();
		if (t == "server")
		{
			getToken(); // consume server
			expectToken("{");
			ServerConfig srv;
			parseServerBlock(srv);
			servers.push_back(srv);
		}
		else
		{
			// Если встретилось что-то неожиданное за пределами server {}
			// Можно проигнорировать или выбросить ошибку
			getToken();
		}
	}
}

void Parser::parseServerBlock(ServerConfig &srv)
{
	while (!isEnd())
	{
		std::string t = peekToken();
		if (t == "}")
		{
			getToken(); // consume }
			break;
		}
		else if (t == "location")
		{
			parseLocationBlock(srv);
		}
		else
		{
			// Директива сервера
			std::string directive = getToken();
			parseServerDirective(srv, directive);
		}
	}
}

void Parser::parseLocationBlock(ServerConfig &srv)
{
	// ожидаем location <path> {
	expectToken("location");
	std::string path = getToken();
	expectToken("{");

	LocationConfig loc;
	loc.path = path;

	while (!isEnd())
	{
		std::string t = peekToken();
		if (t == "}")
		{
			getToken(); // consume }
			break;
		}
		else
		{
			std::string directive = getToken();
			parseLocationDirective(loc, directive);
		}
	}
	srv.locations.push_back(loc);
}

void Parser::parseServerDirective(ServerConfig &srv, const std::string &directive)
{
	if (directive == "listen")
	{
		std::string val = getToken();
		expectToken(";");
		parseListen(srv, val);
	}
	else if (directive == "server_name")
	{
		srv.server_name = getToken();
		expectToken(";");
	}
	else if (directive == "root")
	{
		srv.root = getToken();
		expectToken(";");
	}
	else if (directive == "max_body_size")
	{
		std::string val = getToken();
		expectToken(";");
		srv.max_body_size = parseSize(val);
	}
	else if (directive == "autoindex")
	{
		std::string val = getToken();
		expectToken(";");
		srv.autoindex = (val == "on");
	}
	else if (directive == "error_page")
	{
		std::string code_str = getToken();
		std::string page = getToken();
		expectToken(";");
		int code = parseStatusCode(code_str);
		srv.error_pages[code] = page;
	}
	else if (directive == "methods")
	{
		// methods GET POST DELETE;
		srv.methods.clear();
		while (!isEnd() && peekToken() != ";")
		{
			srv.methods.push_back(getToken());
		}
		expectToken(";");
	}
	else
	{
		// Неизвестная директива - возможно игнорируем или ошибка
		throw std::runtime_error("Unknown server directive: " + directive);
	}
}

void Parser::parseLocationDirective(LocationConfig &loc, const std::string &directive)
{
	if (directive == "root")
	{
		loc.root = getToken();
		expectToken(";");
	}
	else if (directive == "methods")
	{
		loc.methods.clear();
		while (!isEnd() && peekToken() != ";")
		{
			loc.methods.push_back(getToken());
		}
		expectToken(";");
	}
	else if (directive == "autoindex")
	{
		std::string val = getToken();
		expectToken(";");
		loc.autoindex = (val == "on");
	}
	else if (directive == "index")
	{
		loc.index = getToken();
		expectToken(";");
	}
	else if (directive == "cgi_pass")
	{
		loc.cgi_pass = getToken();
		expectToken(";");
	}
	else if (directive == "cgi_extension")
	{
		loc.cgi_extension = getToken();
		expectToken(";");
	}
	else if (directive == "upload_store")
	{
		loc.upload_store = getToken();
		expectToken(";");
	}
	else if (directive == "max_body_size")
	{
		std::string val = getToken();
		expectToken(";");
		loc.max_body_size = parseSize(val);
	}
	else if (directive == "return")
	{
		// Пример: return 301 /newpath;
		std::string code = getToken();
		std::string url = getToken();
		expectToken(";");
		loc.redirect = code + " " + url;
	}
	else
	{
		throw std::runtime_error("Unknown location directive: " + directive);
	}
}

void Parser::parseListen(ServerConfig &srv, const std::string &value)
{
	// Формат: ip:port или просто port
	// Пример: 127.0.0.1:8080
	size_t pos = value.find(':');
	if (pos != std::string::npos)
	{
		srv.host = value.substr(0, pos);
		std::string port_str = value.substr(pos + 1);
		srv.port = atoi(port_str.c_str());
	}
	else
	{
		srv.host = "0.0.0.0";
		srv.port = atoi(value.c_str());
	}
}

size_t Parser::parseSize(const std::string &value)
{
	// Например "200k" или "1m"
	// k = 1024, m = 1024 * 1024
	// Если без суффикса - просто число байт
	// Опционально можно добавить более строгую проверку
	size_t len = value.size();
	if (len == 0)
		throw std::runtime_error("Invalid size value");
	char c = value[len - 1];
	size_t base = std::atoi(value.c_str());
	if (c == 'k' || c == 'K')
	{
		return base * 1024;
	}
	else if (c == 'm' || c == 'M')
	{
		return base * 1024 * 1024;
	}
	else
	{
		return base;
	}
}

int Parser::parseStatusCode(const std::string &value)
{
	int code = atoi(value.c_str());
	if (code < 100 || code > 599)
		throw std::runtime_error("Invalid status code: " + value);
	return code;
}
