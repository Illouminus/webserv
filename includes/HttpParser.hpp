#pragma once
#include <string>
#include "ServerConfig.hpp"
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
	ERR_413,
	ERR_501
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

public:
	HttpParser();
	HttpParser(const HttpParser &other);
	HttpParser &operator=(const HttpParser &other);
	~HttpParser();

	// Add data to the buffer and try to parse it
	void appendData(const std::string &data);

	// Status checkers
	bool isComplete() const;
	bool hasError() const;
	ParserError getErrorCode() const;

	// Check if the headers are parsed
	bool headersComplete() const;
	bool serverSelected() const;
	bool serverIsChosen() const;
	void setServerSelected(bool val);
	void setChosenServer(const ServerConfig &srv);
	const ServerConfig *getChosenServer() const;

	void setMaxBodySize(size_t sz);
    size_t getMaxBodySize() const;
	
	// Getters
	ParserStatus getStatus() const;
	HttpMethod getMethod() const;
	std::string getPath() const;
	std::string getVersion() const;
	std::string getQuery() const;

	std::map<std::string, std::string> getHeaders() const;
	std::string getBody() const;
	std::string getHeader(const std::string &key) const;
	
	bool isKeepAlive() const;

private:
	void parseRequestLine(const std::string &line);
	void parseHeaderLine(const std::string &line);
	void parseHeaders();
	void parseBody();
	void parseChunkedBody();

	// Chunked body parsing
	long parseHexNumber(const std::string &hexStr);

	// Buffer for incoming data
	std::string _buffer;
	ParserError _errorCode; 

	// Status of the parser
	ParserStatus _status;

	// Result of parsing
	HttpMethod _method;
	std::string _path;
	std::string _host;
	std::string _query;
	std::string _version; 

	std::map<std::string, std::string> _headers;
	const ServerConfig *_chosenServer;
	std::string _body;

	size_t _contentLength;
	
	size_t _maxBodySize;

	bool   _headerParsed;   
    bool   _headersDone;    
    bool   _serverSelected; 
};