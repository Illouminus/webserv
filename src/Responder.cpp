#include "Responder.hpp"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h> // для проверки директории

Responder::Responder() {};
Responder::~Responder() {};

const LocationConfig *Responder::findLocation(const ServerConfig &srv, const std::string &path)
{
	size_t bestMatch = 0;
	const LocationConfig *bestLoc = NULL;

	for (size_t i = 0; i < srv.locations.size(); i++)
	{
		const std::string &locPath = srv.locations[i].path;
		// Сравнение префикса:
		if (path.compare(0, locPath.size(), locPath) == 0)
		{
			if (locPath.size() > bestMatch)
			{
				bestMatch = locPath.size();
				bestLoc = &srv.locations[i];
			}
		}
	}
	return bestLoc;
}

std::string Responder::getContentTypeByExtension(const std::string &path)
{
	// Находим последний '.'
	size_t pos = path.rfind('.');
	if (pos == std::string::npos)
		return "application/octet-stream";
	std::string ext = path.substr(pos);
	// В зависимости от расширения
	if (ext == ".html" || ext == ".htm")
		return "text/html";
	if (ext == ".css")
		return "text/css";
	if (ext == ".js")
		return "application/javascript";
	if (ext == ".jpg" || ext == ".jpeg")
		return "image/jpeg";
	if (ext == ".png")
		return "image/png";
	if (ext == ".gif")
		return "image/gif";
	// Можно добавить ещё
	return "application/octet-stream";
}

bool Responder::isMethodAllowed(HttpMethod method, const ServerConfig &server, const LocationConfig *loc, std::string &allowHeader)
{
	// Соберём список методов
	std::vector<std::string> allowed;
	if (loc && !loc->methods.empty())
		allowed = loc->methods;
	else
		allowed = server.methods;

	// Преобразуем HttpMethod method -> string ("GET", "POST", etc)
	// Условно:
	std::string m;
	switch (method)
	{
	case HTTP_METHOD_GET:
		m = "GET";
		break;
	case HTTP_METHOD_POST:
		m = "POST";
		break;
	case HTTP_METHOD_DELETE:
		m = "DELETE";
		break;
	case HTTP_METHOD_PUT:
		m = "PUT";
		break;
	default:
		m = "UNKNOWN";
	}

	// Соберём allowHeader
	std::ostringstream oss;
	for (size_t i = 0; i < allowed.size(); i++)
	{
		if (i > 0)
			oss << ", ";
		oss << allowed[i];
	}
	allowHeader = oss.str();

	// Проверка, есть ли m в allowed
	for (size_t i = 0; i < allowed.size(); i++)
	{
		if (allowed[i] == m)
			return true;
	}
	// Если метод неизвестный (UNKNOWN), тоже false
	return false;
}

std::string Responder::buildFilePath(const ServerConfig &server, const LocationConfig *loc, const std::string &reqPath)
{
	// Если есть локация и loc->root не пуст, используем её root. Иначе server.root
	std::string rootPath;
	if (loc && !loc->root.empty())
		rootPath = loc->root;
	else
		rootPath = server.root;

	// Если локация существует, нужно убрать из reqPath ту часть, что совпадает с loc->path
	// например, loc->path == "/images", reqPath == "/images/test.png"
	// значит realPart = "/test.png"
	std::string realPart = reqPath;
	if (loc && !loc->path.empty())
	{
		size_t len = loc->path.size();
		realPart = reqPath.substr(len); // если reqPath = "/images/test.png", len=7
	}

	// Склеиваем
	// Если rootPath не заканчивается на '/', можно добавить вручную
	if (!rootPath.empty() && rootPath[rootPath.size() - 1] == '/')
		return rootPath + realPart;
	else
		return rootPath + "/" + realPart;
}

bool Responder::setBodyFromFile(HttpResponse &resp, const std::string &filePath)
{
	std::ifstream ifs(filePath.c_str(), std::ios::binary);
	if (!ifs.is_open())
		return false;
	std::ostringstream oss;
	oss << ifs.rdbuf();
	resp.setBody(oss.str());
	return true;
}

void Responder::handleErrorPage(HttpResponse &resp, ServerConfig &server, int code)
{
	// Ищем в server.error_pages
	if (server.error_pages.find(code) != server.error_pages.end())
	{
		std::string errorPath = server.error_pages[code];
		// Обычно путь может быть абсолютным или относительным root (зависит от логики)
		// Для упрощения предположим, что "error_pages" — абсолютный путь, или же "root + errorPath".
		// Попробуем прочитать
		std::ifstream ifs(errorPath.c_str());
		if (ifs.is_open())
		{
			std::ostringstream oss;
			oss << ifs.rdbuf();
			resp.setBody(oss.str());
			resp.setHeader("Content-Type", "text/html");
		}
	}
}