#include "Responder.hpp"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>

Responder::Responder() {};
Responder::~Responder() {};

HttpResponse Responder::handleRequest(const HttpParser &parser, ServerConfig &server)
{
	HttpResponse resp;

	std::string path = parser.getPath();
	const LocationConfig *loc = findLocation(server, path);

	HttpMethod method = parser.getMethod();
	std::string allowHeader;
	if (!isMethodAllowed(method, server, loc, allowHeader))
	{
		resp.setStatus(405, "Method Not Allowed");
		resp.setHeader("Content-Type", "text/plain");
		resp.setHeader("Allow", allowHeader);
		resp.setBody("405 Method Not Allowed\n");
		handleErrorPage(resp, server, 405);
		return resp;
	}
	if (loc && !loc->redirect.empty())
	{
		std::istringstream iss(loc->redirect);
		int code;
		std::string newPath;
		iss >> code >> newPath;

		resp.setStatus(code, "Redirect");
		resp.setHeader("Location", newPath);
		resp.setHeader("Content-Length", "0");
		return resp;
	}
	if (method == HTTP_METHOD_POST)
		return handlePost(parser, loc, path);
	else if (method == HTTP_METHOD_DELETE)
		return handleDelete(server, loc, path);
	else if (method == HTTP_METHOD_GET)
		return handleGet(server, loc, path);
	//  else if (method == HTTP_METHOD_PUT) etc.

	resp.setStatus(501, "Not Implemented");
	resp.setBody("Method not implemented\n");
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
	size_t pos = path.rfind('.');
	if (pos == std::string::npos)
		return "application/octet-stream";
	std::string ext = path.substr(pos);
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
	return "application/octet-stream";
}

bool Responder::isMethodAllowed(HttpMethod method, const ServerConfig &server, const LocationConfig *loc, std::string &allowHeader)
{
	std::vector<std::string> allowed;
	if (loc && !loc->methods.empty())
		allowed = loc->methods;
	else
		allowed = server.methods;

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

	std::ostringstream oss;
	for (size_t i = 0; i < allowed.size(); i++)
	{
		if (i > 0)
			oss << ", ";
		oss << allowed[i];
	}
	allowHeader = oss.str();
	for (size_t i = 0; i < allowed.size(); i++)
	{
		if (allowed[i] == m)
			return true;
	}
	return false;
}

std::string Responder::buildFilePath(const ServerConfig &server,
												 const LocationConfig *loc,
												 const std::string &reqPath)
{
	std::string rootPath;
	if (loc && !loc->root.empty())
		rootPath = loc->root;
	else
		rootPath = server.root;

	if (rootPath.size() > 1 && rootPath.back() == '/')
		rootPath.erase(rootPath.size() - 1);

	std::string realPart = reqPath;
	if (loc && !loc->path.empty())
	{
		size_t len = loc->path.size();
		if (realPart.size() >= len && realPart.compare(0, len, loc->path) == 0)
			realPart.erase(0, len);
	}

	if (!realPart.empty() && realPart[0] == '/')
		realPart.erase(0, 1);

	std::string filePath;
	if (realPart.empty())
		filePath = rootPath; //  "www"
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
	if (server.error_pages.find(code) != server.error_pages.end())
	{
		std::string rootError = server.root;
		std::string errorPath = server.error_pages[code];
		rootError.append(errorPath);
		std::ifstream ifs(rootError.c_str());
		if (ifs.is_open())
		{
			std::ostringstream oss;
			oss << ifs.rdbuf();
			resp.setBody(oss.str());
			resp.setHeader("Content-Type", "text/html");
		}
	}
}

HttpResponse Responder::handleGet(
	 ServerConfig &server,
	 const LocationConfig *loc,
	 const std::string &reqPath)
{
	HttpResponse resp;

	std::string realFilePath = buildFilePath(server, loc, reqPath);
	struct stat st;
	if (stat(realFilePath.c_str(), &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
		{
			if (loc && !loc->index.empty())
			{
				if (realFilePath[realFilePath.size() - 1] != '/')
					realFilePath += "/";
				realFilePath += loc->index;
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

HttpResponse Responder::handleDelete(
	 ServerConfig &server,
	 const LocationConfig *loc,
	 const std::string &reqPath)
{
	HttpResponse resp;

	// Строим физический путь (как для GET)
	std::string realFilePath = buildFilePath(server, loc, reqPath);

	// Проверяем, существует ли
	struct stat st;
	if (stat(realFilePath.c_str(), &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
		{
			// Запрет удалять директории?
			resp.setStatus(403, "Forbidden");
			resp.setBody("Cannot delete a directory\n");
			return resp;
		}
		// Удаляем
		if (std::remove(realFilePath.c_str()) == 0)
		{
			// OK
			resp.setStatus(200, "OK");
			resp.setBody("File deleted\n");
		}
		else
		{
			resp.setStatus(403, "Forbidden");
			resp.setBody("Failed to delete file\n");
		}
	}
	else
	{
		// Не найден
		resp.setStatus(404, "Not Found");
		resp.setBody("File not found\n");
	}
	return resp;
}

std::string Responder::extractFilename(const std::string &reqPath)
{
	// Пример: reqPath = "/upload/something.jpg"
	// нам нужно "something.jpg"
	// Ищем последний '/'
	size_t pos = reqPath.rfind('/');
	if (pos == std::string::npos)
	{
		// Нет слэша, значит сама строка
		return reqPath;
	}
	// Возвращаем всё после последнего '/'
	return reqPath.substr(pos + 1);
}

HttpResponse Responder::handlePost(const HttpParser &parser,
											  const LocationConfig *loc,
											  const std::string &reqPath)
{
	HttpResponse resp;

	// Проверяем, есть ли в локации upload_store
	if (!loc || loc->upload_store.empty())
	{
		// Не разрешено загружать
		resp.setStatus(403, "Forbidden");
		resp.setBody("Upload not allowed\n");
		return resp;
	}

	// Из reqPath, например "/upload/something.jpg", попробуем извлечь "something.jpg"
	// (упрощенная логика)
	std::string filename = extractFilename(reqPath); // см. пример реализации ниже

	std::cout << "Uploading file: " << filename << "\n";

	// Формируем полный путь: loc->upload_store + "/" + filename
	// Например "/var/www/uploads/something.jpg"
	std::string fullUploadPath = loc->upload_store;

	std::cout << "Full path: " << fullUploadPath << "\n";
	if (!fullUploadPath.empty() && fullUploadPath.back() != '/')
		fullUploadPath += "/";
	fullUploadPath += filename;

	// Записываем body в файл
	std::ofstream ofs(fullUploadPath.c_str(), std::ios::binary);
	if (!ofs.is_open())
	{
		resp.setStatus(500, "Internal Server Error");
		resp.setBody("Cannot open file for writing\n");
		return resp;
	}
	ofs << parser.getBody();
	ofs.close();

	// OK
	resp.setStatus(201, "Created");
	resp.setHeader("Content-Type", "text/plain");
	resp.setBody("File uploaded successfully\n");
	return resp;
}