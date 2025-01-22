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
		return this->makeErrorResponse(405, "Method Not Allowed", server, "Method Not Allowed\n");
	
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

	return makeErrorResponse(501, "Not Implemented", server, "Method not implemented");
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

std::string Responder::buildFilePath(const ServerConfig &server, const LocationConfig *loc, const std::string &reqPath)
{
	std::string rootPath;
	if (loc && !loc->root.empty())
		rootPath = loc->root;
	else
		rootPath = server.root;

	if (rootPath.size() > 1 && rootPath[rootPath.size() - 1] == '/')
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

HttpResponse Responder::handleGet(ServerConfig &server, const LocationConfig *loc, const std::string &reqPath)
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
					return makeErrorResponse(403, "Forbidden", server, "Directory listing is forbidden\n");
			}
			else
			{
				// autoindex? Если off -> 403
				if (!server.autoindex && !(loc && loc->autoindex))
					return makeErrorResponse(403, "Forbidden", server, "Directory listing is forbidden\n");
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
			return makeErrorResponse(403, "Forbidden", server, "Cannot read file\n");
		resp.setStatus(200, "OK");
		std::string ctype = getContentTypeByExtension(realFilePath);
		resp.setHeader("Content-Type", ctype);
	}
	else
		return makeErrorResponse(404, "Not Found", server, "File Not Found\n");
	return resp;
}

HttpResponse Responder::handleDelete(ServerConfig &server, const LocationConfig *loc, const std::string &reqPath)
{
	HttpResponse resp;
	std::string realFilePath = buildFilePath(server, loc, reqPath);

	struct stat st;
	if (stat(realFilePath.c_str(), &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
		{
			resp.setStatus(403, "Forbidden");
			resp.setBody("Cannot delete a directory\n");
			return resp;
		}
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
		resp.setStatus(404, "Not Found");
		resp.setBody("File not found\n");
	}
	return resp;
}

std::string Responder::extractFilename(const std::string &reqPath)
{
	size_t pos = reqPath.rfind('/');
	if (pos == std::string::npos)
		return reqPath;
	return reqPath.substr(pos + 1);
}

HttpResponse Responder::handlePost(const HttpParser &parser, const LocationConfig *loc, const std::string &reqPath)
{
	HttpResponse resp;

	std::cout << loc->upload_store << "\n";
	if (!loc || loc->upload_store.empty())
	{
		resp.setStatus(403, "Forbidden");
		resp.setBody("Upload not allowed\n");
		return resp;
	}
	std::string filename = extractFilename(reqPath);

	std::string fullUploadPath = loc->upload_store;

	if (!fullUploadPath.empty() && fullUploadPath[fullUploadPath.size() - 1] != '/')
		fullUploadPath += "/";
	fullUploadPath += filename;

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



HttpResponse Responder::makeErrorResponse(int code,
                                          const std::string &reason,
                                          const ServerConfig &server,
                                          const std::string &defaultMessage)
{
    HttpResponse resp;
    resp.setStatus(code, reason);
    resp.setHeader("Content-Type", "text/html");

    std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
    if (it != server.error_pages.end())
    {
        std::string errorFilePath = server.root + it->second;
        std::ifstream ifs(errorFilePath.c_str());
        if (ifs.is_open())
        {
            std::ostringstream oss;
            oss << ifs.rdbuf();
            resp.setBody(oss.str());
            return resp;
        }
    }
    std::ostringstream oss;
    oss << "<!DOCTYPE html>\n"
        << "<html>\n"
        << "<head><title>" << code << " " << reason << "</title></head>\n"
        << "<body>\n"
        << "<h1>" << code << " " << reason << "</h1>\n"
        << "<p>" << defaultMessage << "</p>\n"
        << "</body>\n</html>";
    resp.setBody(oss.str());
    return resp;
}
