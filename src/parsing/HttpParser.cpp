#include "HttpParser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iostream>

HttpParser::HttpParser()
 : _errorCode(NO_ERROR),
    _status(PARSING_HEADERS),
   _method(HTTP_METHOD_UNKNOWN),
   _chosenServer(NULL),
   _contentLength(0),
   _maxBodySize(0),
   _headerParsed(false),
   _headersDone(false),
   _serverSelected(false)
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

bool HttpParser::isComplete() const {
    return (_status == COMPLETE);
}

bool HttpParser::hasError() const {
    return (_status == PARSING_ERROR);
}

ParserStatus HttpParser::getStatus() const {
	return _status;
}

HttpMethod HttpParser::getMethod() const {
	return _method;
}

std::string HttpParser::getPath() const {
	return _path;
}

std::string HttpParser::getVersion() const {
	return _version;
}

std::string HttpParser::getQuery() const {
	return _query;
}

bool HttpParser::serverIsChosen() const {
    return (_chosenServer != NULL);
}

std::map<std::string, std::string> HttpParser::getHeaders() const {
	return _headers;
}

std::string HttpParser::getBody() const {
	return _body;
}

bool HttpParser::headersComplete() const {
    return _headersDone;
}

bool HttpParser::serverSelected() const {
    return _serverSelected;
}
void HttpParser::setServerSelected(bool val) {
    _serverSelected = val;
}

void HttpParser::setMaxBodySize(size_t sz) {
    _maxBodySize = sz;
}
size_t HttpParser::getMaxBodySize() const {
    return _maxBodySize;
}

std::string HttpParser::getHeader(const std::string &key) const {
    std::string lowerKey = key;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
    std::map<std::string, std::string>::const_iterator it = _headers.find(lowerKey);
    if (it != _headers.end())
        return it->second;
    return "";
}

void HttpParser::setChosenServer(const ServerConfig &srv) {
    _chosenServer = &srv;
}   

const ServerConfig *HttpParser::getChosenServer() const {
    return _chosenServer;
}

void HttpParser::appendData(const std::string &data)
{
    if (_status == COMPLETE || _status == PARSING_ERROR)
        return; 

	
    _buffer += data;

	if (_status == PARSING_HEADERS)
		parseHeaders();

	if (_status == PARSING_BODY)
		parseBody();

	if(_status == PARSING_CHUNKED)
		parseChunkedBody();
}



void HttpParser::parseHeaders()
{
    // Ищем в _buffer последовательность "\r\n\r\n"
    // которая сигнализирует конец заголовков
    size_t endHeadersPos = _buffer.find("\r\n\r\n");
    if (endHeadersPos == std::string::npos) {
        // ещё не пришли все заголовки
        endHeadersPos = _buffer.find("\n\n");
        if (endHeadersPos == std::string::npos) {
        // ещё не пришли все заголовки
        return;
    }
    }
    

    // Получаем часть (все строки заголовков)
    std::string headersBlock = _buffer.substr(0, endHeadersPos);
    // Убираем их из буфера
   if (_buffer.size() >= endHeadersPos + 4 && 
        _buffer.compare(endHeadersPos, 4, "\r\n\r\n") == 0)
    {
        _buffer.erase(0, endHeadersPos + 4);
    }
    else
    {
        // Значит это "\n\n"
        _buffer.erase(0, endHeadersPos + 2);
    }
    // Теперь разобьём headersBlock по строкам "\r\n"
    std::istringstream iss(headersBlock);
    std::string line;
    bool firstLine = true;
    while (std::getline(iss, line, '\n')) {
        // В getline, если не удалим "\r", могут оставаться символы
        // Уберём \r в конце
        if (!line.empty() && line[line.size()-1] == '\r')
            line.erase(line.size()-1, 1);

        if (firstLine) {
            parseRequestLine(line);
            firstLine = false;
            if (_status == PARSING_ERROR)
                return; // Ошибка в request line
        } else {
            parseHeaderLine(line);
            if (_status == PARSING_ERROR)
                return; // Ошибка в заголовке
        }
    }

    _headersDone = true; // мы закончили читать заголовки

    // Проверяем, есть ли Transfer-Encoding: chunked
    std::string te;
    if (_headers.find("transfer-encoding") != _headers.end())
        te = _headers["transfer-encoding"];
    // Приводим к lower
    std::transform(te.begin(), te.end(), te.begin(), ::tolower);

    if (te == "chunked") {
        // Переходим в режим парсинга chunked
        _status = PARSING_CHUNKED;
        parseChunkedBody(); // вдруг сразу уже есть что-то в _buffer
        return;
    }

    // Иначе, проверим Content-Length
    if (_headers.find("content-length") != _headers.end()) {
        long cl = std::atol(_headers["content-length"].c_str());
        if (cl < 0) {
            std::cout << "ОШИБКА 1 " << std::endl;
            _status = PARSING_ERROR;
            _errorCode = ERR_400;
            return;
        }
        _contentLength = static_cast<size_t>(cl);
        _status = PARSING_BODY;
        parseBody(); // может, часть тела уже пришла
    } else {
        // Нет тела (GET или что-то ещё)
        _status = COMPLETE;
    }
}

void HttpParser::parseRequestLine(const std::string &line)
{
    // Example: "GET /index.html?foo=bar HTTP/1.1"

    std::cout << "parseRequestLine: " << line << std::endl;
    std::istringstream iss(line);

    std::string methodStr, rawPath, versionStr;
    if (!(iss >> methodStr >> rawPath >> versionStr)) 
    {
        _status = PARSING_ERROR;
        std::cout << "ОШИБКА 2 " << std::endl;
        _errorCode = ERR_400;
        return;
    }

    // 1) Determine the method
    if (methodStr == "GET") _method = HTTP_METHOD_GET;
    else if (methodStr == "POST") _method = HTTP_METHOD_POST;
    else if (methodStr == "PUT")  _method = HTTP_METHOD_PUT;
    else if (methodStr == "DELETE") _method = HTTP_METHOD_DELETE;
    else _method = HTTP_METHOD_UNKNOWN;

    // 2) Query string if present
    // rawPath for example "/index.html?foo=bar"
    size_t qPos = rawPath.find('?');
    if (qPos != std::string::npos)
    {
        // All before ? — path
        _path = rawPath.substr(0, qPos);
        // All after ? — query
        _query = rawPath.substr(qPos + 1);
    }
    else
    {
        // Don't have query string
        _path = rawPath;
        _query.clear();
    }

    // 3) Protocol version
    _version = versionStr;
    std::cout << "Version: " << _version << std::endl;
    if (_version != "HTTP/1.1" && _version != "HTTP/1.0")
    {
        _status = PARSING_ERROR;
        std::cout << "ОШИБКА 3 " << std::endl;
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
    std::cout << "parseHeaderLine: " << line << std::endl;
	// For example: "Host: localhost:8080"
	size_t pos = line.find(':');
	if (pos == std::string::npos)
		{
			_status = PARSING_ERROR; // 400 Bad Request;
            std::cout << "ОШИБКА 4 " << std::endl;
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
    if (_buffer.size() >= _contentLength) {
        _body = _buffer.substr(0, _contentLength);
        _buffer.erase(0, _contentLength);
        _status = COMPLETE;
    }

    if (_maxBodySize > 0 && _body.size() > _maxBodySize) {
        _status = PARSING_ERROR;
        _errorCode = ERR_413;
    }
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

void HttpParser::parseChunkedBody()
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

        if (_maxBodySize > 0 && _body.size() > _maxBodySize) {
            _status = PARSING_ERROR;
            _errorCode = ERR_413;
            return;
        }
	}

}
