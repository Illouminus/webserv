#pragma once

#include <string>
#include <map>

class HttpResponse
{
private:
	int _statusCode;
	std::string _reasonPhrase;
	std::map<std::string, std::string> _headers;
	std::string _body;

public:
	HttpResponse();
	~HttpResponse();

	void setStatus(int code, const std::string &reason);
	void setHeader(const std::string &key, const std::string &value);
	bool setBodyFromFile(const std::string &filePath);
	void setBody(const std::string &body);

	// Формирует строку с заголовками + телом
	std::string toString() const;

private:
	// Можно добавить вспомогательные методы, например, для формата
	std::string statusLine() const;
};