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

Responder::Responder() {};
Responder::~Responder() {};

/**
 * handleRequest()
 * The main entry point for generating a response from a parsed request and a server config.
 */

HttpResponse Responder::handleRequest(const HttpParser &parser, ServerConfig &server)
{
	HttpResponse resp;

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
		return handlePost(server, parser, loc, path);
	else if (method == HTTP_METHOD_DELETE)
		return handleDelete(server, loc, path);
	else if (method == HTTP_METHOD_GET)
		return handleGet(server, parser, loc, path);
	//  else if (method == HTTP_METHOD_PUT) etc.

	// For anything else, return 501
	return makeErrorResponse(501, "Not Implemented", server, "Method not implemented");
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
HttpResponse Responder::handleCgi(const ServerConfig &server, const HttpParser &parser,
											 const LocationConfig *loc,
											 const std::string &reqPath)
{
	HttpResponse resp;

	// 1) Build the full script path (physical path)
	//    E.g. "/var/www/cgi-bin/script.py", using your buildFilePath(...) if needed.
	std::string scriptPath = buildFilePath(server, loc, reqPath);

	// 2) Construct environment variables
	std::vector<std::string> envVec;

	// REQUEST_METHOD
	{
		std::string methodStr;
		switch (parser.getMethod())
		{
		case HTTP_METHOD_GET:
			methodStr = "GET";
			break;
		case HTTP_METHOD_POST:
			methodStr = "POST";
			break;
		case HTTP_METHOD_DELETE:
			methodStr = "DELETE";
			break;
		case HTTP_METHOD_PUT:
			methodStr = "PUT";
			break;
		default:
			methodStr = "UNKNOWN";
		}
		envVec.push_back("REQUEST_METHOD=" + methodStr);
	}

	// CONTENT_LENGTH if POST, or maybe parser.getHeaders()["content-length"] if present
	if (parser.getMethod() == HTTP_METHOD_POST)
	{
		std::ostringstream oss;
		oss << parser.getBody().size(); // chunked or not, final size is parser.getBody().size()
		envVec.push_back("CONTENT_LENGTH=" + oss.str());
	}

	// QUERY_STRING if GET (split from path ?)
	// E.g. if reqPath = "/cgi-bin/script.py?x=1&y=2", parse query.
	// In your parser, you might store these.
	// For simplicity, let's do nothing or parse if you have them.

	// SCRIPT_NAME (the path to the CGI script in URL), PATH_INFO, etc. (optional)
	// SERVER_PROTOCOL, e.g. "HTTP/1.1"
	envVec.push_back("SERVER_PROTOCOL=" + parser.getVersion());

	// Convert envVec to null-terminated array of char*
	std::vector<char *> envp;
	for (size_t i = 0; i < envVec.size(); i++)
	{
		envp.push_back(const_cast<char *>(envVec[i].c_str()));
	}
	envp.push_back(NULL);

	// 3) Build args for execve: [cgi_pass, scriptPath, NULL]
	//    e.g. ["/usr/bin/python", "/var/www/cgi-bin/script.py", NULL]
	std::vector<char *> args;
	args.push_back(const_cast<char *>(loc->cgi_pass.c_str())); // e.g. /usr/bin/python
	args.push_back(const_cast<char *>(scriptPath.c_str()));	  // actual script
	args.push_back(NULL);

	// 4) Prepare pipes for child's stdout -> parent reading
	int pipeOut[2];
	if (pipe(pipeOut) == -1)
		return makeErrorResponse(500, "Internal Server Error", server, "Failed to create pipeOut\n");

	// Optionally create pipe for stdin if there's a request body
	bool hasBody = (!parser.getBody().empty());
	int pipeIn[2];
	if (hasBody)
	{
		if (pipe(pipeIn) == -1)
		{
			close(pipeOut[0]);
			close(pipeOut[1]);
			return makeErrorResponse(500, "Internal Server Error", server, "Failed to create pipeIn\n");
		}
	}

	// 5) Fork
	pid_t pid = fork();
	if (pid < 0)
	{
		// Fork failed
		close(pipeOut[0]);
		close(pipeOut[1]);
		if (hasBody)
		{
			close(pipeIn[0]);
			close(pipeIn[1]);
		}
		return makeErrorResponse(500, "Internal Server Error", server, "Fork failed\n");
	}
	else if (pid == 0)
	{
		// --- CHILD PROCESS ---

		// Redirect stdout to pipeOut[1]
		dup2(pipeOut[1], STDOUT_FILENO);
		close(pipeOut[0]); // not needed in child
		close(pipeOut[1]);

		// If we have body, redirect stdin from pipeIn[0]
		if (hasBody)
		{
			dup2(pipeIn[0], STDIN_FILENO);
			close(pipeIn[1]);
			close(pipeIn[0]);
		}

		// execve: cgi_pass + script
		// envp for environment
		execve(args[0], &args[0], &envp[0]);

		// If execve fails:
		_exit(1);
	}
	else
	{
		// --- PARENT PROCESS ---
		// We do not write to pipeOut[1]
		close(pipeOut[1]);

		// If we have body, let's write parser.getBody() to child's stdin
		if (hasBody)
		{
			close(pipeIn[0]);
			// Write the entire request body
			ssize_t written = write(pipeIn[1],
											parser.getBody().c_str(),
											parser.getBody().size());
			(void)written; // ignore or check for errors
			close(pipeIn[1]);
		}

		// Now read child's stdout from pipeOut[0]
		std::ostringstream cgiOutput;
		char buf[1024];
		ssize_t rd;
		while ((rd = read(pipeOut[0], buf, sizeof(buf))) > 0)
		{
			cgiOutput.write(buf, rd);
		}
		close(pipeOut[0]);

		// Wait for child to finish
		int status;
		waitpid(pid, &status, 0);

		// Build the HttpResponse
		std::string outStr = cgiOutput.str();

		// Minimal approach: treat all CGI output as body
		// Or parse "Content-type: ..." etc.
		resp.setStatus(200, "OK");
		resp.setBody(outStr);
		resp.setHeader("Content-Type", "text/html");
		// In a more advanced solution, we'd parse CGI headers from outStr.

		return resp;
	}

	// If we somehow get here, return an error
	return makeErrorResponse(500, "Internal Server Error", server, "Unknown CGI error\n");
}
