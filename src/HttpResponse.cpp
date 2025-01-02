#include "HttpResponse.hpp"
#include <sstream>
#include <fstream>

HttpResponse::HttpResponse()
	 : _statusCode(200), _reasonPhrase("OK")
{
	// По умолчанию 200 OK, можно установить потом вручную
}

HttpResponse::~HttpResponse() {}

void HttpResponse::setStatus(int code, const std::string &reason)
{
	_statusCode = code;
	_reasonPhrase = reason;
}

void HttpResponse::setHeader(const std::string &key, const std::string &value)
{
	_headers[key] = value;
}

bool HttpResponse::setBodyFromFile(const std::string &filePath)
{
	std::ifstream ifs(filePath.c_str(), std::ios::binary);
	if (!ifs.is_open())
	{
		// Ошибка открытия файла
		return false;
	}
	// Считываем весь файл в _body (для учебных целей)
	std::ostringstream oss;
	oss << ifs.rdbuf();
	_body = oss.str();

	// Автоматически можем поставить Content-Length
	std::ostringstream cl;
	cl << _body.size();
	_headers["Content-Length"] = cl.str();

	return true;
}

void HttpResponse::setBody(const std::string &body)
{
	_body = body;
	// Обновим Content-Length
	std::ostringstream oss;
	oss << _body.size();
	_headers["Content-Length"] = oss.str();
}

std::string HttpResponse::statusLine() const
{
	// "HTTP/1.1 200 OK"
	std::ostringstream oss;
	oss << "HTTP/1.1 " << _statusCode << " " << _reasonPhrase << "\r\n";
	return oss.str();
}