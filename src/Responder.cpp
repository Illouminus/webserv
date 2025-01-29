#include "Responder.hpp"
#include "AutoIndex.cpp"
#include "Outils.cpp"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>

Responder::Responder() {};
Responder::~Responder() {};
std::map<std::string, std::string> Responder::g_sessions;

/**
 * handleRequest()
 * The main entry point for generating a response from a parsed request and a server config.
 */

HttpResponse Responder::handleRequest(const HttpParser &parser, ServerConfig &server)
{
    HttpResponse resp;
	bool needSetCookie = false;
	std::string cookieStr;
	std::string newSid;
	
    // 0) (Optional) parse cookie
    std::map<std::string, std::string> cookies;
    {
        // We look for the Cookie header
        std::map<std::string, std::string> headers = parser.getHeaders(); 
        // 'cookie' is lowercased in parseHeaderLine
        if (headers.find("cookie") != headers.end()) {
            cookieStr = headers["cookie"];
        }
        // parse
        cookies = parseCookieString(cookieStr);
    }

    // 0.1) Check if we have session_id
    if (cookies.find("session_id") == cookies.end())
    {
        // not found => generate
		needSetCookie = true;
        newSid = generateRandomSessionID();
        // store in global map 
        this->g_sessions[newSid] = "Some user data or empty string";
        // (optional) we can do a debug print
        std::cout << "New session_id created: " << newSid << std::endl;
    }
    else
    {
        // We do have session_id
        std::string sid = cookies["session_id"];
        std::cout << "Got session_id = " << sid << std::endl;
        // if you want, check g_sessions[sid]
    }

    // 1) Find matching location
    std::string path = parser.getPath();

	const LocationConfig *loc = findLocation(server, path);

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

const LocationConfig *Responder::findLocation(const ServerConfig &srv, const std::string &path)
{
	size_t bestMatch = 0;
	const LocationConfig *bestLoc = NULL;

	for (size_t i = 0; i < srv.locations.size(); i++)
	{
		const std::string &locPath = srv.locations[i].path;
		// If path begins with locPath
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

HttpResponse Responder::handleGet(ServerConfig &server, const HttpParser &parser,
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
					return makeErrorResponse(403, "Forbidden", server, "Directory listing is forbidden\n");
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
		std::cout << "File not found: " << realFilePath << std::endl;
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
	// Check if CGI
	if (loc && !loc->cgi_pass.empty())
	{
		// Possibly check extension, or just do it:
		return handleCgi(server, parser, loc, reqPath);
	}

	HttpResponse resp;
	// If there's an upload store => save body as a file
	if (!loc || loc->upload_store.empty())
	{
		resp.setStatus(403, "Forbidden");
		resp.setBody("Upload not allowed\n");
		return resp;
	}

	std::string filename = extractFilename(reqPath);
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
	ofs << parser.getBody();
	ofs.close();

	resp.setStatus(201, "Created");
	resp.setHeader("Content-Type", "text/plain");
	resp.setBody("File uploaded successfully\n");
	return resp;
}

HttpResponse Responder::handleDelete(ServerConfig &server, const LocationConfig *loc, const std::string &reqPath)
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
    HttpResponse resp;

    // 1) Build the script path on disk, e.g. "/var/www/site1/cgi-bin/test.php"
    std::string scriptPath = buildFilePath(server, loc, reqPath);

	std::cout << "CGI script path: " << scriptPath << std::endl;
    
    // 2) Detect interpreter
    //    Option A: If loc->cgi_pass is not empty => use that
    //    Option B: Or check extension
    std::string cgiInterpreter;

    size_t dotPos = scriptPath.rfind('.');
    if (dotPos == std::string::npos)
    {
        return makeErrorResponse(403, "Forbidden", server,
                                     "No CGI extension found\n");
    }
    std::string ext = scriptPath.substr(dotPos); // e.g. ".php" or ".sh"

    if (ext == ".php")
    {
        cgiInterpreter = "/usr/bin/php"; // or "/usr/bin/php-cgi" /opt/homebrew/bin/php-cg
    }
    else if (ext == ".sh")
    {
        cgiInterpreter = "/bin/sh";
    }
    else
    {
        return makeErrorResponse(403, "Forbidden", server,
                                     "Unsupported CGI extension\n");
    }
    

    // 3) Build environment variables (envVec)
    std::vector<std::string> envVec;

    // (a) REQUEST_METHOD
    {
        std::string methodStr;
        switch (parser.getMethod())
        {
            case HTTP_METHOD_GET:    methodStr = "GET";    break;
            case HTTP_METHOD_POST:   methodStr = "POST";   break;
            case HTTP_METHOD_DELETE: methodStr = "DELETE"; break;
            case HTTP_METHOD_PUT:    methodStr = "PUT";    break;
            default:                 methodStr = "UNKNOWN";
        }
        envVec.push_back("REQUEST_METHOD=" + methodStr);
    }

    // (b) CONTENT_LENGTH, CONTENT_TYPE (if POST/PUT)
    if (parser.getMethod() == HTTP_METHOD_POST || parser.getMethod() == HTTP_METHOD_PUT)
    {
        // content-length
        std::ostringstream oss;
        oss << parser.getBody().size();
        envVec.push_back("CONTENT_LENGTH=" + oss.str());

        // content-type (use parser.getHeaders() if you want the real one)
        // For now, we hardcode to text/plain
        envVec.push_back("CONTENT_TYPE=text/plain");
    }

    // (c) SERVER_PROTOCOL
    envVec.push_back("SERVER_PROTOCOL=" + parser.getVersion()); // e.g. "HTTP/1.1"

    // (d) SCRIPT_FILENAME (often needed by php-cgi)
    envVec.push_back("SCRIPT_FILENAME=" + scriptPath);

    // (e) QUERY_STRING — we assume parser has getQuery()
    // if not, you can do your own parse of ? in path
    envVec.push_back("QUERY_STRING=" + parser.getQuery());

    // (f) Some CGI variables that php-cgi typically requires
    envVec.push_back("GATEWAY_INTERFACE=CGI/1.1");
    envVec.push_back("REDIRECT_STATUS=200");

    // Optionally: SERVER_NAME, SERVER_SOFTWARE, etc. could be added
    // envVec.push_back("SERVER_NAME=localhost");
    // envVec.push_back("SERVER_SOFTWARE=webserv/1.0");

    // 4) Convert envVec -> envp (null-terminated array of char*)
    std::vector<char *> envp;
    for (size_t i = 0; i < envVec.size(); i++)
    {
        envp.push_back(const_cast<char *>(envVec[i].c_str()));
    }
    envp.push_back(NULL);

    // 5) Build args array: [cgiInterpreter, scriptPath, NULL]
    std::vector<char *> args;
    args.push_back(const_cast<char *>(cgiInterpreter.c_str()));
    args.push_back(const_cast<char *>(scriptPath.c_str()));
    args.push_back(NULL);

    // 6) Create pipes
    int pipeOut[2];
    if (pipe(pipeOut) == -1)
    {
        return makeErrorResponse(500, "Internal Server Error", server,
                                 "Failed to create pipeOut\n");
    }

    bool hasBody = !parser.getBody().empty();
    int pipeIn[2];
    if (hasBody)
    {
        if (pipe(pipeIn) == -1)
        {
            close(pipeOut[0]);
            close(pipeOut[1]);
            return makeErrorResponse(500, "Internal Server Error", server,
                                     "Failed to create pipeIn\n");
        }
    }

    // 7) Fork
    pid_t pid = fork();
    if (pid < 0)
    {
        // fork failed
        close(pipeOut[0]);
        close(pipeOut[1]);
        if (hasBody)
        {
            close(pipeIn[0]);
            close(pipeIn[1]);
        }
        return makeErrorResponse(500, "Internal Server Error", server,
                                 "Fork failed\n");
    }
    else if (pid == 0)
    {
        // -- Child process --

        // Redirect STDOUT to pipeOut[1]
        dup2(pipeOut[1], STDOUT_FILENO);
        close(pipeOut[0]);
        close(pipeOut[1]);

        // If we have request body, tie pipeIn[0] -> STDIN
        if (hasBody)
        {
            dup2(pipeIn[0], STDIN_FILENO);
            close(pipeIn[1]);
            close(pipeIn[0]);
        }

        // execve
        execve(args[0], &args[0], &envp[0]);
        // If execve fails, exit child
        _exit(1);
    }
    else
    {
        // -- Parent process --

        // We don't write to pipeOut[1]
        close(pipeOut[1]);

        if (hasBody)
        {
            // Write the request body to child's stdin
            close(pipeIn[0]);
            write(pipeIn[1], parser.getBody().c_str(), parser.getBody().size());
            close(pipeIn[1]);
        }

        // Read child's stdout
        std::ostringstream cgiOutput;
        char buf[1024];
        ssize_t rd;
        while ((rd = read(pipeOut[0], buf, sizeof(buf))) > 0)
        {
            cgiOutput.write(buf, rd);
        }
        close(pipeOut[0]);

        // Wait for child
        int status;
        waitpid(pid, &status, 0);

        // Build response
        HttpResponse resp;
        resp.setStatus(200, "OK");
        resp.setHeader("Content-Type", "text/html");
        resp.setBody(cgiOutput.str());

        return resp;
    }

    // Should not reach here
    return makeErrorResponse(500, "Internal Server Error", server,
                             "Unknown CGI error\n");
}
