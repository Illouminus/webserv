#include "HttpParser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iostream>

HttpParser::HttpParser()
	 : _status(PARSING_HEADERS),
		_method(HTTP_METHOD_UNKNOWN),
		_contentLength(0),
		_headerParsed(false)
{
}

HttpParser::HttpParser(const HttpParser &other)
{
	*this = other;
}

HttpParser::~HttpParser() {}

HttpParser &HttpParser::operator=(const HttpParser &other)
{
	if (this != &other)
	{
		_buffer = other._buffer;
		_status = other._status;
		_method = other._method;
		_path = other._path;
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
		_contentLength = other._contentLength;
		_headerParsed = other._headerParsed;
	}
	return *this;
}

void HttpParser::appendData(const std::string &data, size_t maxBodySize)
{
	_buffer += data;

	if (_buffer.size() > maxBodySize)
	{
		_status = PARSING_ERROR;
		return;
	}

	if (_status == PARSING_HEADERS)
		parseHeaders();

	if (_status == PARSING_BODY)
		parseBody();
}

bool HttpParser::isComplete() const
{
	return (_status == COMPLETE);
}

ParserStatus HttpParser::getStatus() const
{
	return _status;
}

HttpMethod HttpParser::getMethod() const
{
	return _method;
}

std::string HttpParser::getPath() const
{
	return _path;
}

std::string HttpParser::getVersion() const
{
	return _version;
}

std::map<std::string, std::string> HttpParser::getHeaders() const
{
	return _headers;
}

std::string HttpParser::getBody() const
{
	return _body;
}

void HttpParser::parseHeaders()
{
	// Find position of \r\n\r\n, which separates headers from body
	while (true)
	{
		// Looking for the first occurence of \r\n
		size_t pos = _buffer.find("\r\n");
		if (pos == std::string::npos)
		{
			_status = PARSING_ERROR;
			return;
		}

		// If \r\n is at the beginning of the buffer, then headers are parsed
		if (pos == 0)
		{
			// Errase "\r\n" from the buffer
			_buffer.erase(0, 2);

			// Headers are parsed and we can check if there is a body
			// If there is a body, then we need to parse it
			// Else we can set the status to COMPLETE and return
			if (_headers.count("content-length"))
			{
				_contentLength = static_cast<size_t>(std::atoi(_headers["content-length"].c_str()));
				_status = PARSING_BODY;
				parseBody(); 
			}
			else
			{
				// Don't have content length (GET probably), so we can set the status to COMPLETE
				_status = COMPLETE;
			}
			return;
		}

		// Иначе считываем строку (line) до \r\n
		std::string line = _buffer.substr(0, pos);
		_buffer.erase(0, pos + 2);

		// Если не разобрана первая строка (Request Line) — парсим её
		if (!_headerParsed)
		{
			parseRequestLine(line);
			_headerParsed = true;
		}
		else
		{
			// Иначе это заголовок "Key: Value"
			parseHeaderLine(line);
		}
	}
}

void HttpParser::parseRequestLine(const std::string &line)
{
	// Пример: "GET /index.html HTTP/1.1"
	// Разобьём по пробелам
	std::istringstream iss(line);
	std::string methodStr, pathStr, versionStr;
	iss >> methodStr >> pathStr >> versionStr;

	// Определяем метод
	if (methodStr == "GET")
		_method = HTTP_METHOD_GET;
	else if (methodStr == "POST")
		_method = HTTP_METHOD_POST;
	else if (methodStr == "PUT")
		_method = HTTP_METHOD_PUT;
	else if (methodStr == "DELETE")
		_method = HTTP_METHOD_DELETE;
	else
		_method = HTTP_METHOD_UNKNOWN;

	_path = pathStr;
	_version = versionStr; // "HTTP/1.1"
}

bool HttpParser::isKeepAlive() const
{
	// если _version == "HTTP/1.1", то по умолчанию keep-alive, если не указано close
	// если _version == "HTTP/1.0", то по умолчанию close, если не указано keep-alive

	std::map<std::string, std::string>::const_iterator it = _headers.find("connection");

	std::string connectionVal;
	if (it != _headers.end())
	{
		connectionVal = it->second;
		// tolower
		std::transform(connectionVal.begin(), connectionVal.end(), connectionVal.begin(), ::tolower);
	}

	// Для упрощения, примем логику:
	if (_version == "HTTP/1.1")
	{
		if (connectionVal == "close")
			return false;
		// иначе keep-alive
		return true;
	}
	else // HTTP/1.0
	{
		if (connectionVal == "keep-alive")
			return true;
		return false;
	}
}

void HttpParser::parseHeaderLine(const std::string &line)
{
	// Пример: "Host: localhost:8080"
	size_t pos = line.find(':');
	if (pos == std::string::npos)
		return; // Невалидный заголовок или неформатный; в реальном сервере -> 400 Bad Request

	std::string key = line.substr(0, pos);
	std::string value = line.substr(pos + 1);

	// Trim пробелы
	// Можно написать helper-функцию trim
	// trim trailing whitespace (аналог pop_back()):
	while (!key.empty() && std::isspace(static_cast<unsigned char>(key[key.size() - 1])))
	{
		key.erase(key.size() - 1, 1);
	}

	// trim leading whitespace (аналог front()):
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value[0])))
	{
		value.erase(0, 1);
	}

	// Приводим key в нижний регистр для удобства (часто делают так)
	std::transform(key.begin(), key.end(), key.begin(), ::tolower);

	_headers[key] = value;
}

void HttpParser::parseBody()
{
	// Если уже есть достаточное количество байт для тела, дочитываем
	if (_buffer.size() >= _contentLength)
	{
		_body = _buffer.substr(0, _contentLength);
		_buffer.erase(0, _contentLength);

		// Поскольку дочитали тело, завершаем
		_status = COMPLETE;
	}
	// Иначе ждем следующего вызова appendData()
}

bool HttpParser::hasError() const
{
	return (_status == PARSING_ERROR);
}