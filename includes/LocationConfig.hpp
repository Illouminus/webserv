#pragma once
#include <string>
#include <vector>

class LocationConfig
{
public:
	LocationConfig();
	LocationConfig(const LocationConfig &other);
	LocationConfig &operator=(const LocationConfig &other);
	~LocationConfig();

	std::string path;
	std::string root;
	std::vector<std::string> methods;
	bool autoindex;
	std::string index;
	std::string cgi_pass;
	std::string cgi_extension;
	std::string upload_store;
	std::string redirect; // Например: "301 /newpath"
	size_t max_body_size;

	void reset();

	std::string getRoot() const;
};
