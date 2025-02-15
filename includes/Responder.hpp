#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include "HttpParser.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
#include "Outils.hpp"

class Responder
{
public:
	Responder();
	~Responder();

	// The main method of the class: handle the request and return the response
	HttpResponse handleRequest(const HttpParser &parser, const ServerConfig &server);

	const LocationConfig *findLocation(const ServerConfig &server, const HttpParser &parser);

	HttpResponse makeErrorResponse(int code, const std::string &reason,
											 const ServerConfig &server,
											 const std::string &defaultMessage);
	Outils outils;

private:
	
	static std::map<std::string, std::string> g_sessions;
	std::string getContentTypeByExtension(const std::string &path);
	bool isMethodAllowed(HttpMethod method, const ServerConfig &server, const LocationConfig *loc, std::string &allowHeader);
	std::string buildFilePath(const ServerConfig &server, const LocationConfig *loc, const std::string &path);
	bool setBodyFromFile(HttpResponse &resp, const std::string &filePath);
	HttpResponse handleGet(const ServerConfig &server, const HttpParser &parser, const LocationConfig *loc, const std::string &reqPath);
	HttpResponse handlePost(const ServerConfig &server, const HttpParser &parser, const LocationConfig *loc, const std::string &reqPath);
	HttpResponse handleDelete(const ServerConfig &server, const LocationConfig *loc, const std::string &reqPath);
	std::string extractFilename(const std::string &reqPath);
	HttpResponse handleCgi(const ServerConfig &server, const HttpParser &parser, const LocationConfig *loc, const std::string &reqPath);
	HttpResponse processCgiOutput(int pipeFd, pid_t childPid);
	bool parseMultipartFormData(const std::string &contentType, const std::string &body, std::string &fileFieldName, std::string &filename, std::string &fileContent);
};