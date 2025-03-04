#include "Responder.hpp"
#include "AutoIndex.cpp"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <iostream>

Responder::Responder() {
    Outils outils;
};
Responder::~Responder() {};
std::map<std::string, std::string> Responder::g_sessions;

/**
 * handleRequest()
 * The main entry point for generating a response from a parsed request and a server config.
 */

HttpResponse Responder::handleRequest(const HttpParser &parser, const ServerConfig &server)
{
    HttpResponse resp;
    bool needSetCookie = false;
    std::string newSid;

    // 0) Get headers
    std::map<std::string, std::string> headers = parser.getHeaders();

    // 1) Parse cookies
    std::string cookieStr;
    if (headers.find("cookie") != headers.end()) {
        cookieStr = headers["cookie"];
    }
    std::map<std::string, std::string> cookies = outils.parseCookieString(cookieStr);

    // 2) Verify session
    std::string sid;
    if (cookies.find("session_id") == cookies.end())
    {
        // No session cookie found - create a new session
        needSetCookie = true;
        newSid = outils.generateRandomSessionID();
        
    }
    else
    {
        // Have a session cookie - (In real life and project - check if it's valid)
        sid = cookies["session_id"];
        const SessionData* sd = outils.getSessionData(sid);
        if (sd)
        {
            // Console log
            // std::cout << "[Session] Welcome back! Last IP: " << sd->ip
            //           << ", last visit: " << ctime(&(sd->lastVisit)) 
            //           << ", userAgent: " << sd->userAgent << std::endl;
        }
        else
        {
            // If we cleared the session data (in our next project / have to set expire time for session)
            // Again if session is not valid (in our next project / have to set expire time for session)- create a new session
            needSetCookie = true;
            newSid = outils.generateRandomSessionID();
        }
    }
    std::string clientIP = parser.getClientIP();  
    std::string userAgent;
    if (headers.find("user-agent") != headers.end()) {
        userAgent = headers["user-agent"];
    }

    if (needSetCookie)
    {
        sid = newSid;
        resp.setHeader("Set-Cookie", "session_id=" + sid + "; Path=/; HttpOnly");
    }

    outils.storeSessionData(sid, clientIP, userAgent);


    // 1) Find matching location
    std::string path = parser.getPath();

	const LocationConfig *loc = findLocation(server, parser);

    size_t max_body_size;
    // check if body size is too big


    if(server.max_body_size > 0 && loc->max_body_size == 0)
        max_body_size = server.max_body_size;
    else 
        max_body_size = loc->max_body_size;
    


    if (max_body_size > 0 && parser.getBody().size() > max_body_size)
    {
        return this->makeErrorResponse(413, "Request Entity Too Large", server, "Request Entity Too Large\n");
    }


    // 2) Check if method is allowed
    HttpMethod method = parser.getMethod();
    std::string allowHeader;
    if (!isMethodAllowed(method, server, loc, allowHeader))
        return this->makeErrorResponse(405, "Method Not Allowed", server, "Method Not Allowed\n");

    // 3) Check if there's a redirect in the location
    if (loc && !loc->redirect.empty())
    {
        // e.g. "301 /newpath"
        std::istringstream iss(loc->redirect);
        int code;
        std::string newPath;
        iss >> code >> newPath;

        resp.setStatus(code, "Redirect");
        resp.setHeader("Location", newPath);
        resp.setHeader("Content-Length", "0");
        return resp;
    }

    // 4) Dispatch by method
    if (method == HTTP_METHOD_POST)
        resp = handlePost(server, parser, loc, path);
    else if (method == HTTP_METHOD_DELETE)
        resp = handleDelete(server, loc, path);
    else if (method == HTTP_METHOD_GET)
        resp = handleGet(server, parser, loc, path);
	else
 		return makeErrorResponse(501, "Not Implemented", server, "Method not implemented");

	if (needSetCookie)
		resp.setHeader("Set-Cookie", "session_id=" + newSid + "; Path=/; HttpOnly");

	return resp;
   
}


/**
 * findLocation()
 * Finds the best matching location for the given path.
 */

const LocationConfig *Responder::findLocation(const ServerConfig &srv, const HttpParser &parser)
{
    std::string path = parser.getPath();
    HttpMethod method = parser.getMethod();

    // Translate HttpMethod enum to string
    std::string methodStr;
    switch (method)
    {
        case HTTP_METHOD_GET:    methodStr = "GET"; break;
        case HTTP_METHOD_POST:   methodStr = "POST"; break;
        case HTTP_METHOD_PUT:    methodStr = "PUT"; break;
        case HTTP_METHOD_DELETE: methodStr = "DELETE"; break;
        default:                 methodStr = "";
    }

    // First check if the path has an extension that matches a CGI location
    std::string ext = outils.extractExtention(path);

    if(!ext.empty())
    {
            for (size_t i = 0; i < srv.locations.size(); i++)
    {
        if (!srv.locations[i].cgi_extension.empty() && srv.locations[i].cgi_extension == ext)
        {
            // If location has methods, check if the method is allowed
            if (!srv.locations[i].methods.empty())
            {
                bool allowed = false;
                for (size_t j = 0; j < srv.locations[i].methods.size(); j++)
                {
                    if (srv.locations[i].methods[j] == methodStr)
                    {
                        allowed = true;
                        break;
                    }
                }
                if (!allowed)
                {
                    // Method not allowed for this location
                    continue;
                }
            }
            // Finally, return the CGI location
            return &srv.locations[i];
        }
    }
    }

    // If no CGI location found, check the path prefix
    size_t bestMatch = 0;
    const LocationConfig *bestLoc = NULL;
    for (size_t i = 0; i < srv.locations.size(); i++)
    {
        const std::string &locPath = srv.locations[i].path;
        if (path.compare(0, locPath.size(), locPath) == 0)
        {
            if (locPath.size() > bestMatch)
            {
                bestMatch = locPath.size();
                bestLoc = &srv.locations[i];
            }
        }
    }
    return bestLoc;
}

/**
 * getContentTypeByExtension()
 * Returns a Content-Type based on the file's extension
 */

std::string Responder::getContentTypeByExtension(const std::string &path)
{
	size_t pos = path.rfind('.');
	if (pos == std::string::npos)
		return "application/octet-stream";
	std::string ext = path.substr(pos);
	if (ext == ".html" || ext == ".htm")
		return "text/html";
	if (ext == ".css")
		return "text/css";
	if (ext == ".js")
		return "application/javascript";
	if (ext == ".jpg" || ext == ".jpeg")
		return "image/jpeg";
	if (ext == ".png")
		return "image/png";
	if (ext == ".gif")
		return "image/gif";
	return "application/octet-stream";
}

/**
 * isMethodAllowed()
 * Checks if the given HTTP method is in either location->methods or server.methods.
 */
bool Responder::isMethodAllowed(HttpMethod method, const ServerConfig &server, const LocationConfig *loc, std::string &allowHeader)
{
	// Gather allowed methods
	std::vector<std::string> allowed;
	if (loc && !loc->methods.empty())
		allowed = loc->methods;
	else
		allowed = server.methods;
        

	// Convert HttpMethod enum to string
	std::string m;
	switch (method)
	{
	case HTTP_METHOD_GET:
		m = "GET";
		break;
	case HTTP_METHOD_POST:
		m = "POST";
		break;
	case HTTP_METHOD_DELETE:
		m = "DELETE";
		break;
	case HTTP_METHOD_PUT:
		m = "PUT";
		break;
	default:
		m = "UNKNOWN";
	}

	// Build the Allow header string
	std::ostringstream oss;
	for (size_t i = 0; i < allowed.size(); i++)
	{
		if (i > 0)
			oss << ", ";
		oss << allowed[i];
	}

	// Check if method is in 'allowed'
	allowHeader = oss.str();
	for (size_t i = 0; i < allowed.size(); i++)
	{
		if (allowed[i] == m)
			return true;
	}
	return false;
}

/**
 * buildFilePath()
 * Constructs the physical path on disk based on server.root or loc->root,
 * plus the request path (minus location prefix).
 */

std::string Responder::buildFilePath(const ServerConfig &server, const LocationConfig *loc, const std::string &reqPath)
{

	// Decide root
	std::string rootPath;
	if (loc && !loc->root.empty())
		rootPath = loc->root;
	else
		rootPath = server.root;

	// Remove trailing slash if root is bigger than just "/"
	if (rootPath.size() > 1 && rootPath[rootPath.size() - 1] == '/')
		rootPath.erase(rootPath.size() - 1);

	// Remove location prefix from reqPath, if present
	std::string realPart = reqPath;
	if (loc && !loc->path.empty())
	{
		size_t len = loc->path.size();
		if (realPart.size() >= len && realPart.compare(0, len, loc->path) == 0)
			realPart.erase(0, len);
	}

	// Remove leading slash
	if (!realPart.empty() && realPart[0] == '/')
		realPart.erase(0, 1);

	// Combine root and realPart
	std::string filePath;
	if (realPart.empty())
		filePath = rootPath; //  "www"
	else
		filePath = rootPath + "/" + realPart; // "www/index.html"
	return filePath;
}

/**
 * setBodyFromFile()
 * Attempts to read the entire file into resp.body.
 * Returns false if file not found or not readable.
 */
bool Responder::setBodyFromFile(HttpResponse &resp, const std::string &filePath)
{
	std::ifstream ifs(filePath.c_str(), std::ios::binary);
	if (!ifs.is_open())
		return false;
	std::ostringstream oss;
	oss << ifs.rdbuf();
	resp.setBody(oss.str());
	return true;
}

/**
 * extractFilename()
 * Utility method to get the file component from a path (e.g. "/upload/foo.txt" => "foo.txt").
 */
std::string Responder::extractFilename(const std::string &reqPath)
{
	size_t pos = reqPath.rfind('/');
	if (pos == std::string::npos)
		return reqPath;
	return reqPath.substr(pos + 1);
}

/**
 * makeErrorResponse()
 * Builds an HTTP error response with either a custom error_page (if configured)
 * or a default HTML body.
 */

HttpResponse Responder::makeErrorResponse(int code,
														const std::string &reason,
														const ServerConfig &server,
														const std::string &defaultMessage)
{
	HttpResponse resp;
	resp.setStatus(code, reason);
	resp.setHeader("Content-Type", "text/html");

	// Check if there's a user-defined error_page for this code
	std::map<int, std::string>::const_iterator it = server.error_pages.find(code);
	if (it != server.error_pages.end())
	{
		// Attempt to open server.root + that error_page path
		std::string errorFilePath = server.root + it->second;
		std::ifstream ifs(errorFilePath.c_str());
		if (ifs.is_open())
		{
			std::ostringstream oss;
			oss << ifs.rdbuf();
			resp.setBody(oss.str());
			return resp;
		}
	}

	// Otherwise, build a default HTML error page
	std::ostringstream oss;
	oss << "<!DOCTYPE html>\n"
		 << "<html>\n"
		 << "<head><title>" << code << " " << reason << "</title></head>\n"
		 << "<body>\n"
		 << "<h1>" << code << " " << reason << "</h1>\n"
		 << "<p>" << defaultMessage << "</p>\n"
		 << "</body>\n</html>";
	resp.setBody(oss.str());
	return resp;
}

//======================================================================
//         MAIN HANDLERS: GET, POST, DELETE (plus potential CGI check)
//======================================================================

/**
 * handleGet()
 *  - if it's a directory and we have loc->index, try that index file
 *  - else if directory and autoindex is on => generate autoindex
 *  - else serve static file
 *  - or if loc->cgi_pass => handleCgi(...)
 */

HttpResponse Responder::handleGet(const ServerConfig &server, const HttpParser &parser,
											 const LocationConfig *loc,
											 const std::string &reqPath)
{
	HttpResponse resp;

	// (Optionally) check if this is a CGI request:
	if (loc && !loc->cgi_pass.empty())
	{
		return handleCgi(server, parser, loc, reqPath);
	}

	// Otherwise, proceed as static file or autoindex
	std::string realFilePath = buildFilePath(server, loc, reqPath);


	struct stat st;
	if (stat(realFilePath.c_str(), &st) == 0)
	{
		// Directory => check index or autoindex
		if (S_ISDIR(st.st_mode))
		{
			if (loc && !loc->index.empty())
			{
				// e.g. "index.html"
				if (realFilePath[realFilePath.size() - 1] != '/')
					realFilePath += "/";
				realFilePath += loc->index;

				struct stat st2;
				if (stat(realFilePath.c_str(), &st2) != 0 || S_ISDIR(st2.st_mode))
				{
					// Index not found or is a dir
					return makeErrorResponse(404, "Not found", server, "Directory listing is forbidden\n");
				}
			}
			else
			{
				// No index => autoindex?
				if (!server.autoindex && !(loc && loc->autoindex))
				{
					return makeErrorResponse(403, "Forbidden", server, "Directory listing is forbidden\n");
				}
				else
				{
					// Generate autoindex page
					std::string html = makeAutoIndexPage(realFilePath, reqPath);
					HttpResponse resp;
					resp.setStatus(200, "OK");
					resp.setHeader("Content-Type", "text/html");
					resp.setBody(html);
					return resp;
				}
			}
		}

		// Serve static file
		if (!setBodyFromFile(resp, realFilePath))
			return makeErrorResponse(403, "Forbidden", server, "Cannot read file\n");

		resp.setStatus(200, "OK");
		std::string ctype = getContentTypeByExtension(realFilePath);
		resp.setHeader("Content-Type", ctype);
	}
	else
	{
		return makeErrorResponse(404, "Not Found", server, "File Not Found\n");
	}
	return resp;
}

/**
 * handlePost()
 *  - If location has cgi_pass => call handleCgi(...)
 *  - Else if location has upload_store => store file
 *  - Otherwise => 403 or method not allowed
 */

HttpResponse Responder::handlePost(const ServerConfig &server, const HttpParser &parser, const LocationConfig *loc, const std::string &reqPath)
{
    // Если есть CGI-передача, вызываем её:
    if (loc && !loc->cgi_pass.empty())
    {
        return handleCgi(server, parser, loc, reqPath);
    }

    HttpResponse resp;


    std::string dirPath = loc->upload_store;
            
    if (!dirPath.empty() && dirPath[dirPath.size() - 1] != '/')
        dirPath += "/";

    struct stat dirStat;
    if (stat(dirPath.c_str(), &dirStat) != 0 || !S_ISDIR(dirStat.st_mode))
    {
        return makeErrorResponse(404, "Not Found", server, "Upload directory not found\n");
    }
    

    if (!loc || loc->upload_store.empty())
    {
        
        resp.setStatus(403, "Forbidden");
        resp.setBody("Upload not allowed\n");
        return resp;
    }

    // Parsing multipart/form-data
    std::string contentType = parser.getHeader("Content-Type");
    std::string fileData;
    std::string fileFieldName;
    std::string filename;
    bool isMultipart = (contentType.find("multipart/form-data") != std::string::npos);

    if (isMultipart)
    {
        // Parse multipart form data
        if (!parseMultipartFormData(contentType, parser.getBody(), fileFieldName, filename, fileData))
        {
            resp.setStatus(400, "Bad Request");
            resp.setBody("Failed to parse multipart form data\n");
            return resp;
        }
    }
    else
    {
        // If not multipart, the body is the file data
        fileData = parser.getBody();
        // can extract filename from path
        filename = extractFilename(reqPath);
    }

    // If no filename, use a default:
    if (filename.empty())
    {
        filename = "upload.dat";
    }

    // Form full path to store the file
    std::string fullUpload = loc->upload_store;
    if (!fullUpload.empty() && fullUpload[fullUpload.size() - 1] != '/')
        fullUpload += "/";
    fullUpload += filename;

    std::ofstream ofs(fullUpload.c_str(), std::ios::binary);
    if (!ofs.is_open())
    {
        resp.setStatus(500, "Internal Server Error");
        resp.setBody("Cannot open file for writing\n");
        return resp;
    }
    ofs << fileData;
    ofs.close();

    resp.setStatus(201, "Created");
    resp.setHeader("Content-Type", "text/plain");
    resp.setBody("File uploaded successfully\n");
    return resp;
}

HttpResponse Responder::handleDelete(const ServerConfig &server, const LocationConfig *loc, const std::string &reqPath)
{
	HttpResponse resp;
	std::string realFilePath = buildFilePath(server, loc, reqPath);

	struct stat st;
	if (stat(realFilePath.c_str(), &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
		{
			resp.setStatus(403, "Forbidden");
			resp.setBody("Cannot delete a directory\n");
			return resp;
		}
		if (std::remove(realFilePath.c_str()) == 0)
		{
			// OK
			resp.setStatus(200, "OK");
			resp.setBody("File deleted\n");
		}
		else
		{
			resp.setStatus(403, "Forbidden");
			resp.setBody("Failed to delete file\n");
		}
	}
	else
	{
		resp.setStatus(404, "Not Found");
		resp.setBody("File not found\n");
	}
	return resp;
}

/**
 * handleCgi()
 * Launches an external CGI script, passing the HTTP request data via environment variables
 * and (if POST) via stdin. Reads the script's stdout, then forms an HttpResponse.
 *
 * Requirements:
 *  - loc->cgi_pass contains path to the interpreter (e.g. "/usr/bin/python")
 *  - scriptPath is the actual path to the .py or .php file on disk.
 *  - We set basic environment variables: REQUEST_METHOD, CONTENT_LENGTH, QUERY_STRING, etc.
 *
 * Returns: HttpResponse with the CGI output, or an error (500, etc.)
 */

HttpResponse Responder::handleCgi(const ServerConfig &server, 
                                  const HttpParser &parser,
                                  const LocationConfig *loc,
                                  const std::string &reqPath)
{


    HttpMethod method = parser.getMethod();
    if (method != HTTP_METHOD_GET && method != HTTP_METHOD_POST) {
        return makeErrorResponse(405, "Method Not Allowed", server, "Method Not Allowed for CGI\n");
    }
    
    // 1) Build full path to the script
    std::string scriptPath = buildFilePath(server, loc, reqPath);

    // 2) Choise CGI interpreter based on extension
    std::string cgiInterpreter;
    size_t dotPos = scriptPath.rfind('.');
    if (dotPos == std::string::npos) {
        return makeErrorResponse(403, "Forbidden", server, "No CGI extension found\n");
    }
    std::string ext = scriptPath.substr(dotPos);
    if (loc->cgi_extension == ext) {
        cgiInterpreter = loc->cgi_pass;
    } else {
        return makeErrorResponse(403, "Forbidden", server, "Unsupported CGI extension\n");
    }

    // 3) Create environment variables
    std::vector<std::string> envVec;
    {
        std::string methodStr;
        switch (parser.getMethod())
        {
            case HTTP_METHOD_GET:    methodStr = "GET";    break;
            case HTTP_METHOD_POST:   methodStr = "POST";   break;
            case HTTP_METHOD_DELETE: methodStr = "DELETE"; break;
            case HTTP_METHOD_PUT:    methodStr = "PUT";    break;
            default:                 methodStr = "UNKNOWN"; break;
        }
        envVec.push_back("REQUEST_METHOD=" + methodStr);
    }
    if (parser.getMethod() == HTTP_METHOD_POST || parser.getMethod() == HTTP_METHOD_PUT)
    {
        std::ostringstream oss;
        oss << parser.getBody().size();
        envVec.push_back("CONTENT_LENGTH=" + oss.str());
        envVec.push_back("CONTENT_TYPE=text/plain");
    }
    envVec.push_back("SERVER_PROTOCOL=" + parser.getVersion());
    envVec.push_back("SCRIPT_FILENAME=" + scriptPath);
    envVec.push_back("QUERY_STRING=" + parser.getQuery());
    envVec.push_back("GATEWAY_INTERFACE=CGI/1.1");
    envVec.push_back("REDIRECT_STATUS=200");

    // 4) Convert envVec to char* array
    std::vector<char *> envp;
    for (size_t i = 0; i < envVec.size(); i++) {
        envp.push_back(const_cast<char *>(envVec[i].c_str()));
    }
    envp.push_back(NULL);

    // 5) Form the argument list for execve
    std::vector<char *> args;
    args.push_back(const_cast<char *>(cgiInterpreter.c_str()));
    args.push_back(const_cast<char *>(scriptPath.c_str()));
    args.push_back(NULL);

    // 6) Create pipes for communication with the CGI script
    int pipeOut[2];
    if (pipe(pipeOut) == -1) {
        return makeErrorResponse(500, "Internal Server Error", server, "Failed to create pipeOut\n");
    }
    bool hasBody = !parser.getBody().empty();
    int pipeIn[2];
    if (hasBody) {
        if (pipe(pipeIn) == -1) {
            close(pipeOut[0]);
            close(pipeOut[1]);
            return makeErrorResponse(500, "Internal Server Error", server, "Failed to create pipeIn\n");
        }
    }

    // 7) Fork
    pid_t pid = fork();
    if (pid < 0) {
        close(pipeOut[0]);
        close(pipeOut[1]);
        if (hasBody) {
            close(pipeIn[0]);
            close(pipeIn[1]);
        }
        return makeErrorResponse(500, "Internal Server Error", server, "Fork failed\n");
    }
    else if (pid == 0) {
        // Child process - set up pipes and run the script
        dup2(pipeOut[1], STDOUT_FILENO);
        dup2(pipeOut[1], STDERR_FILENO);
        close(pipeOut[0]);
        close(pipeOut[1]);

        if (hasBody) {
            dup2(pipeIn[0], STDIN_FILENO);
            close(pipeIn[1]);
            close(pipeIn[0]);
        }

        // Run the script
        execve(args[0], &args[0], &envp[0]);
        // If execve returns, it failed
        _exit(1);
    }
    else {
        // Parent process - close unused pipe ends
        close(pipeOut[1]);  
        if (hasBody) {
            close(pipeIn[0]);
            write(pipeIn[1], parser.getBody().c_str(), parser.getBody().size());
            close(pipeIn[1]);
        }
        // Function to read the output of the CGI script (if our script acctually writes something)
        HttpResponse response = processCgiOutput(pipeOut[0], pid);
        return response;
    }

    return makeErrorResponse(500, "Internal Server Error", server, "Unknown CGI error\n");
}


HttpResponse Responder::processCgiOutput(int pipeFd, pid_t childPid)
{
    // 1. Read the output of the CGI script
    std::ostringstream cgiOutput;
    char buf[1024];
    ssize_t bytesRead;
    while ((bytesRead = read(pipeFd, buf, sizeof(buf))) > 0) {
        cgiOutput.write(buf, bytesRead);
    }
    close(pipeFd);

    // 2. Wait for the child process to finish
    int status;
    waitpid(childPid, &status, 0);
    std::string output = cgiOutput.str();

    // If process exited abnormally, return 500 with the output as the body.
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        HttpResponse err;
        err.setStatus(500, "Internal Server Error");
        err.setHeader("Content-Type", "text/html");
        std::ostringstream errBody;
        errBody << "<html><body><h1>CGI Execution Error</h1>"
                << "<pre>" << output << "</pre></body></html>";
        err.setBody(errBody.str());
        // Set Content-Length
        std::ostringstream oss;
        oss << errBody.str().size();
        err.setHeader("Content-Length", oss.str());
        return err;
    }

    // 3. Parsing the output of the CGI script as an HTTP response message.
    // So ussualy we have headers and body devide by empty line
    std::istringstream iss(output);
    std::string line;
    std::map<std::string, std::string> headers;
    std::string body;

    while (std::getline(iss, line)) {
        // if the string is ended with '\r' we remove it
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        // Empty line means end of headers
        if (line == "")
            break;

        // Split the line into key and value
        std::string::size_type pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // Remove leading spaces from value
            while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
                value.erase(0, 1);
            headers[key] = value;
        }
    }

    // The rest of the output is the body
    std::ostringstream bodyStream;
    bodyStream << iss.rdbuf();
    body = bodyStream.str();

    // 4. Erase Content-Length if it's present in the headers
    if (headers.find("Content-Length") != headers.end())
        headers.erase("Content-Length");

    // 5. Form the HttpResponse object
    HttpResponse resp;
    int statusCode = 200;
    std::string reasonPhrase = "OK";

    // if we have Status header we should parse it
    if (headers.find("Status") != headers.end()) {
        std::istringstream statusLine(headers["Status"]);
        statusLine >> statusCode;
        std::getline(statusLine, reasonPhrase);
        while (!reasonPhrase.empty() && (reasonPhrase[0] == ' ' || reasonPhrase[0] == '\t'))
            reasonPhrase.erase(0, 1);
        headers.erase("Status");
    }
    resp.setStatus(statusCode, reasonPhrase);

    // Set headers
    for (std::map<std::string, std::string>::iterator it = headers.begin();
         it != headers.end(); ++it) {
        resp.setHeader(it->first, it->second);
    }

    // Set body
    resp.setBody(body);

    // Set Content-Length with the real body size
    std::ostringstream oss;
    oss << body.size();
    resp.setHeader("Content-Length", oss.str());

    return resp;
}





bool Responder::parseMultipartFormData(const std::string &contentType,
                            const std::string &body,
                            std::string &fileFieldName,
                            std::string &filename,
                            std::string &fileContent)
{
    // Extract boundary from Content-Type header
    // Expected format: "multipart/form-data; boundary=----WebKitFormBoundaryAQRTcpQXQ6fB9YDJ"
    size_t pos = contentType.find("boundary=");
    if (pos == std::string::npos) {
        std::cerr << "No boundary found in Content-Type header" << std::endl;
        return false;
    }
    std::string boundary = contentType.substr(pos + 9);
    boundary = outils.trim(boundary);
    // if boundary is quoted, remove the quotes
    if (!boundary.empty() && boundary[0]=='"') {
        boundary.erase(0,1);
        if (!boundary.empty() && boundary[boundary.size()-1]=='"')
            boundary.erase(boundary.size()-1);
    }
    // the full boundary string starts with "--"
    std::string fullBoundary = "--" + boundary;
    // the closing boundary string ends with "--"
    std::string closingBoundary = fullBoundary + "--";

    // use std::istringstream to parse the body line by line
    std::istringstream iss(body);
    std::string line;
    bool inPart = false;
    bool headersParsed = false;
    std::string partHeaders;
    std::string partBody;
    bool filePartFound = false;
    
    while (std::getline(iss, line)) {
        // remove '\r' at the end of the line
        if (!line.empty() && line[line.size()-1] == '\r')
            line.erase(line.size()-1);

        // if the line is the full boundary or the closing boundary, it means the end of the current part
        if (line == fullBoundary || line == closingBoundary) {
            // if we were in a part and the headers were parsed, we have a complete part
            if (inPart && headersParsed) {
                // if partsHEads has Content-Disposition, we have a file part
                size_t cdPos = partHeaders.find("Content-Disposition:");
                if (cdPos != std::string::npos) {
                    // For example
                    // Content-Disposition: form-data; name="myfile"; filename="test.txt"
                    std::istringstream headerStream(partHeaders);
                    std::string headerLine;
                    while (std::getline(headerStream, headerLine, ';')) {
                        headerLine = outils.trim(headerLine);
                        if (headerLine.find("name=") != std::string::npos) {
                            size_t posQuote = headerLine.find('"');
                            if (posQuote != std::string::npos) {
                                size_t posQuote2 = headerLine.find('"', posQuote+1);
                                if (posQuote2 != std::string::npos) {
                                    fileFieldName = headerLine.substr(posQuote+1, posQuote2-posQuote-1);
                                }
                            }
                        }
                        if (headerLine.find("filename=") != std::string::npos) {
                            size_t posQuote = headerLine.find('"');
                            if (posQuote != std::string::npos) {
                                size_t posQuote2 = headerLine.find('"', posQuote+1);
                                if (posQuote2 != std::string::npos) {
                                    filename = headerLine.substr(posQuote+1, posQuote2-posQuote-1);
                                }
                            }
                        }
                    }
                    // if we have a filename, we have a file part
                    if (!filename.empty()) {
                        fileContent = partBody;
                        filePartFound = true;
                        break;  // we have found the file part, we can stop parsing
                    }
                }
            }
            // reset the part variables and continue parsing
            inPart = true;
            headersParsed = false;
            partHeaders = "";
            partBody = "";
            continue;
        }

        // If we are in a part, we can parse headers and body
        if (inPart) {
            // if headers are not parsed yet, we are still in the headers section
            if (!headersParsed) {
                if (line == "") {
                    // empty line means end of headers
                    headersParsed = true;
                } else {
                    partHeaders += line + "\r\n";
                }
            } else {
                // headers are parsed, we are in the body section and we add the line to the body
                partBody += line + "\n";
            }
        }
    }

    if (filePartFound) {
        // trim the last '\n' from the body
        fileContent = outils.trim(fileContent);
        return true;
    } else {
        return false;
    }
}