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
#include "HttpParser.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"

class Responder
{
public:
	Responder();
	~Responder();

	// Главная точка входа: на вход - уже распарсенный запрос и ссылка на ServerConfig
	// Возвращаем готовый HttpResponse
	HttpResponse handleRequest(const HttpParser &parser, const ServerConfig &server);

	HttpResponse makeErrorResponse(int code, const std::string &reason,
											 const ServerConfig &server,
											 const std::string &defaultMessage);

private:
	
	static std::map<std::string, std::string> g_sessions;
	// Вспомогательная функция: находим нужную LocationConfig*
	const LocationConfig *findLocation(const ServerConfig &server, const std::string &path);

	// Можно добавить метод для определения Content-Type по расширению
	std::string getContentTypeByExtension(const std::string &path);

	// Проверка, разрешён ли метод
	bool isMethodAllowed(HttpMethod method, const ServerConfig &server, const LocationConfig *loc, std::string &allowHeader);

	// Сформировать путь к файлу (с учётом root, location, index и т.д.)
	std::string buildFilePath(const ServerConfig &server, const LocationConfig *loc, const std::string &path);

	// Попытка отдать статический файл (или вернуть false, если не получилось)
	bool setBodyFromFile(HttpResponse &resp, const std::string &filePath);

	HttpResponse handleGet(const ServerConfig &server, const HttpParser &parser,
								  const LocationConfig *loc, const std::string &reqPath);
	HttpResponse handlePost(const ServerConfig &server,
									const HttpParser &parser, const LocationConfig *loc, const std::string &reqPath);
	HttpResponse handleDelete(const ServerConfig &server,
									  const LocationConfig *loc, const std::string &reqPath);

	std::string extractFilename(const std::string &reqPath);

	HttpResponse handleCgi(const ServerConfig &server, const HttpParser &parser,
								  const LocationConfig *loc,
								  const std::string &reqPath);

	HttpResponse processCgiOutput(int pipeFd, pid_t childPid);
};