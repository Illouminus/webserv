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