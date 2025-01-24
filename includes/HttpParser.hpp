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
	PARSING_CHUNKED,
	COMPLETE,
	PARSING_ERROR
};

class HttpParser
{
private:
	// Buffer for incoming data
	std::string _buffer;
	ParserError _errorCode; 

	// Status of the parser
	ParserStatus _status;

	// Result of parsing
	HttpMethod _method;
	std::string _path;
	std::string _version; // For example, "HTTP/1.1"
	std::map<std::string, std::string> _headers;
	std::string _body;

	size_t _contentLength; // If mentioned in headers
	bool _headerParsed;	  // Flag about first line of request parsed

	// size_t _contentSize;

public:
	HttpParser();
	HttpParser(const HttpParser &other);
	HttpParser &operator=(const HttpParser &other);
	~HttpParser();

	// Add data to the buffer and try to parse it
	void appendData(const std::string &data, size_t maxBodySize);

	// Check if the parsing is complete 
	bool isComplete() const;

	// Check if the connection should be kept alive
	bool isKeepAlive() const;

	// Getters
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
	void parseChunkedBody();
};