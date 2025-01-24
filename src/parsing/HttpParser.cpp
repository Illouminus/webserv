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
		_errorCode = ERR_413;
		return;
	}

	if (_status == PARSING_HEADERS)
		parseHeaders();

	if (_status == PARSING_BODY)
		parseBody();

	if(_status == PARSING_CHUNKED)
		parseChunkedBody(maxBodySize);
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
			_errorCode = ERR_400;
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

			if(_headers.count("transfer-encoding") && _headers["transfer-encoding"] == "chunked")
			{
				_status = PARSING_CHUNKED;
				//parseChunkedBody();
				return;
			}

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

		// Learn the header line on the left of \r\n
		std::string line = _buffer.substr(0, pos);
		_buffer.erase(0, pos + 2);

		// If the request line is not parsed yet, then parse it
		if (!_headerParsed)
		{
			parseRequestLine(line);
			_headerParsed = true;
		}
		else
		{
			// Otherwise this is a header line KEY : VALUE
			parseHeaderLine(line);
		}
	}
}

void HttpParser::parseRequestLine(const std::string &line)
{
	// Example: "GET /index.html HTTP/1.1"
	std::istringstream iss(line);
	std::string methodStr, pathStr, versionStr;
	if (!(iss >> methodStr >> pathStr >> versionStr)) {
   		_status = PARSING_ERROR;
		_errorCode = ERR_400;
    	return;
}
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
	if (versionStr != "HTTP/1.1" && versionStr != "HTTP/1.0") {
   		_status = PARSING_ERROR;
		_errorCode = ERR_400;
    	return;
}
}

bool HttpParser::isKeepAlive() const
{
	//  _version == "HTTP/1.1", default keep-alive, if not specified close
	//  _version == "HTTP/1.0", default close, if not specified keep-alive
	std::map<std::string, std::string>::const_iterator it = _headers.find("connection");

	std::string connectionVal;
	if (it != _headers.end())
	{
		connectionVal = it->second;
		// tolower
		std::transform(connectionVal.begin(), connectionVal.end(), connectionVal.begin(), ::tolower);
	}

	if (_version == "HTTP/1.1")
	{
		if (connectionVal == "close")
			return false;
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
	// For example: "Host: localhost:8080"
	size_t pos = line.find(':');
	if (pos == std::string::npos)
		{
			_status = PARSING_ERROR; // 400 Bad Request;
			_errorCode = ERR_400;
			return;
		}

	std::string key = line.substr(0, pos);
	std::string value = line.substr(pos + 1);
	// trim trailing whitespace
	while (!key.empty() && std::isspace(static_cast<unsigned char>(key[key.size() - 1])))
		key.erase(key.size() - 1, 1);
	// trim leading whitespace
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value[0])))
		value.erase(0, 1);

	// To lowercase
	std::transform(key.begin(), key.end(), key.begin(), ::tolower);

	_headers[key] = value;
}

void HttpParser::parseBody()
{
	if (_buffer.size() >= _contentLength)
	{
		_body = _buffer.substr(0, _contentLength);
		_buffer.erase(0, _contentLength);

		_status = COMPLETE;
	}
}

bool HttpParser::hasError() const
{
	return (_status == PARSING_ERROR);
}

ParserError HttpParser::getErrorCode() const
{
	return _errorCode;
}


long HttpParser::parseHexNumber(const std::string &hexStr)
{
    char *endptr = NULL;
    long val = std::strtol(hexStr.c_str(), &endptr, 16);
    if (*endptr != '\0') {
        // Not a valid hex number
        return -1;
    }
    if (val < 0) {
        // Negative number
        return -1;
    }
    return val;
}

void HttpParser::parseChunkedBody(size_t maxBodySize)
{
	
	while(_status == PARSING_CHUNKED)
	{
		// 1) Find the position of \r\n for the chunk size

		size_t posRN = _buffer.find("\r\n");

		if(posRN == std::string::npos)
		 return ; // wait for more data

		// 2) Get the chunk size in hex

		std::string hexLen = _buffer.substr(0, posRN);
		_buffer.erase(0, posRN + 2); // Erase the chunk size and \r\n

		// 3) Convert the hex string to size_t

		long chunkSize = parseHexNumber(hexLen);

		if(chunkSize < 0)
		{
			_status = PARSING_ERROR;
			_errorCode = ERR_501;
			return;
		}

		// 4) If chunk size is 0, then this is the last chunk

		if(chunkSize == 0)
		{
			_status = COMPLETE;
			return;
		}

		// 5) Check if the buffer has enough data to read the chunk

		if(_buffer.size() < static_cast<size_t>(chunkSize) + 2)
			return; // wait for more data

		// 6) Read the chunk and erase it from the buffer

		std::string chunkData = _buffer.substr(0, chunkSize);
        _buffer.erase(0, chunkSize);

		if (_buffer.size() < 2)
        {
            // The chunk should end with \r\n
            _status = PARSING_ERROR;
            _errorCode = ERR_400;
            return;
        }

		if (_buffer[0] != '\r' || _buffer[1] != '\n')
        {
            // Format error
            _status = PARSING_ERROR;
            _errorCode = ERR_400;
            return;
        }

		_buffer.erase(0, 2); // Erase the \r\n

		// 7) Append the chunk data to the body

		_body += chunkData;

		// 8) Check if the body size is not too big

		if(_body.size() > maxBodySize)
		{
			_status = PARSING_ERROR;
			_errorCode = ERR_413;
			return;
		}
	}

}