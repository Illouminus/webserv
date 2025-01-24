#pragma once
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <vector>

enum HttpMethod
{
	HTTP_METHOD_UNKNOWN = 0,
	HTTP_METHOD_GET,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_DELETE,
};

enum ParserError {
	NO_ERROR,
	ERR_400,
	ERR_413
};

enum ParserStatus
{
	PARSING_HEADERS,
	PARSING_BODY,
	COMPLETE,
	PARSING_ERROR
};

class HttpParser
{
private:
	// Накопительный буфер для входящих данных
	std::string _buffer;
	ParserError _errorCode; 

	// Текущее состояние парсера
	ParserStatus _status;

	// Результат парсинга
	HttpMethod _method;
	std::string _path;
	std::string _version; // Например, "HTTP/1.1"
	std::map<std::string, std::string> _headers;
	std::string _body;

	size_t _contentLength; // Если указано в заголовках
	bool _headerParsed;	  // Флаг, что первая строка (Request Line) уже разобрана

	// size_t _contentSize;

public:
	HttpParser();
	HttpParser(const HttpParser &other);
	HttpParser &operator=(const HttpParser &other);
	~HttpParser();

	// Добавляет кусок данных и пытается распарсить
	void appendData(const std::string &data, size_t maxBodySize);

	// Проверяем, закончен ли парсинг (полностью)
	bool isComplete() const;
	bool isKeepAlive() const;

	// Геттеры для результата парсинга
	ParserStatus getStatus() const;
	HttpMethod getMethod() const;
	std::string getPath() const;
	std::string getVersion() const;
	std::map<std::string, std::string> getHeaders() const;
	std::string getBody() const;
	bool hasError() const;
	ParserError getErrorCode() const;

private:
	void parseRequestLine(const std::string &line);
	void parseHeaderLine(const std::string &line);
	void parseHeaders();
	void parseBody();
};