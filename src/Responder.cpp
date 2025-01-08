#include "Responder.hpp"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h> // для проверки директории

Responder::Responder() {};
Responder::~Responder() {};

HttpResponse Responder::handleRequest(const HttpParser &parser, ServerConfig &server)
{
	HttpResponse resp;

	// 1) Найдём локацию
	std::string path = parser.getPath();
	std::cout << path << std::endl;
	const LocationConfig *loc = findLocation(server, path);

	// 2) Проверим метод
	// Сформируем список методов, разрешённых в loc->methods (если не пусто), иначе server.methods
	HttpMethod method = parser.getMethod();

	// Соберём строку Allow (например "GET, POST, DELETE")
	std::string allowHeader;
	if (!isMethodAllowed(method, server, loc, allowHeader))
	{
		resp.setStatus(405, "Method Not Allowed");
		resp.setHeader("Content-Type", "text/plain");
		resp.setHeader("Allow", allowHeader); // «Allow: GET, POST, ...»
		resp.setBody("405 Method Not Allowed\n");
		// Попробуем подставить свою error_page[405], если хотите
		handleErrorPage(resp, server, 405);
		return resp;
	}

	// 3) Если у локации есть redirect (return 301 /newpath):
	// Предположим loc->redirect == "301 /newpath"
	if (loc && !loc->redirect.empty())
	{
		// Парсим "301 /newpath"
		// это возможно "302 /other" etc
		std::istringstream iss(loc->redirect);
		int code;
		std::string newPath;
		iss >> code >> newPath;

		resp.setStatus(code, "Redirect");
		resp.setHeader("Location", newPath);
		resp.setHeader("Content-Length", "0");
		return resp;
	}

	// 4) Строим физический путь
	std::string realFilePath = buildFilePath(server, loc, path);
	std::cout << "realFilePath: " << realFilePath << std::endl;

	// 5) Проверим, существует ли файл
	// Если это директория, и autoindex on/off и index...
	// (Упрощённо ниже)
	struct stat st;
	if (stat(realFilePath.c_str(), &st) == 0)
	{
		// Проверка, не является ли директорией
		if (S_ISDIR(st.st_mode))
		{
			// Если loc->index не пуст, попробуем attach index
			if (loc && !loc->index.empty())
			{
				if (realFilePath[realFilePath.size() - 1] != '/')
					realFilePath += "/";
				realFilePath += loc->index;
				// проверим снова stat
				if (stat(realFilePath.c_str(), &st) == 0 && !S_ISDIR(st.st_mode))
				{
					// ок, есть index
				}
				else
				{
					// Возможно autoindex
					// Или 403/404
					resp.setStatus(403, "Forbidden");
					resp.setBody("Directory listing is forbidden\n");
					handleErrorPage(resp, server, 403);
					return resp;
				}
			}
			else
			{
				// autoindex? Если off -> 403
				if (!server.autoindex && !(loc && loc->autoindex))
				{
					resp.setStatus(403, "Forbidden");
					resp.setBody("Directory listing is forbidden\n");
					handleErrorPage(resp, server, 403);
					return resp;
				}
				else
				{
					// сделать autoindex listing
					// (пример не реализован)
					resp.setStatus(200, "OK");
					resp.setHeader("Content-Type", "text/html");
					resp.setBody("<html><body><h1>Index of directory ...</h1></body></html>");
					return resp;
				}
			}
		}
		// Файл существует, попытаемся отдать
		if (!setBodyFromFile(resp, realFilePath))
		{
			// не смогли прочитать
			resp.setStatus(403, "Forbidden");
			resp.setBody("Cannot read file\n");
			handleErrorPage(resp, server, 403);
			return resp;
		}

		resp.setStatus(200, "OK");
		// Определим Content-Type по расширению
		std::string ctype = getContentTypeByExtension(realFilePath);
		resp.setHeader("Content-Type", ctype);
	}
	else
	{
		// Файл не найден
		resp.setStatus(404, "Not Found");
		resp.setBody("File Not Found\n");
		handleErrorPage(resp, server, 404);
	}

	return resp;
}

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

std::string Responder::buildFilePath(const ServerConfig &server,
												 const LocationConfig *loc,
												 const std::string &reqPath)
{
	// 1) Определяем root
	std::string rootPath;
	if (loc && !loc->root.empty())
		rootPath = loc->root;
	else
		rootPath = server.root;

	// Удаляем завершающий '/', кроме случая когда rootPath == "/"
	if (rootPath.size() > 1 && rootPath.back() == '/')
	{
		rootPath.erase(rootPath.size() - 1);
	}

	// 2) Уберём loc->path из reqPath (префикс)
	std::string realPart = reqPath; // например "/index.html"
	if (loc && !loc->path.empty())
	{
		size_t len = loc->path.size();
		// Убедимся, что path действительно начинается на loc->path
		if (realPart.size() >= len && realPart.compare(0, len, loc->path) == 0)
		{
			realPart.erase(0, len); // вырезаем префикс
		}
	}

	// 3) Удаляем ведущий '/' из realPart, если есть
	// (чтобы не получить двойные слэши при склейке)
	if (!realPart.empty() && realPart[0] == '/')
	{
		realPart.erase(0, 1);
	}

	// 4) Cклеиваем: rootPath + "/" + realPart
	// если realPart пустой => получится rootPath
	std::string filePath;
	if (realPart.empty())
		filePath = rootPath; // например "www"
	else
		filePath = rootPath + "/" + realPart; // "www/index.html"

	return filePath;
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