#include "LocationConfig.hpp"

LocationConfig::LocationConfig() : autoindex(false), max_body_size(0) {}
LocationConfig::LocationConfig(const LocationConfig &other)
{
	*this = other;
}
LocationConfig &LocationConfig::operator=(const LocationConfig &other)
{
	if (this != &other)
	{
		path = other.path;
		root = other.root;
		methods = other.methods;
		autoindex = other.autoindex;
		index = other.index;
		cgi_pass = other.cgi_pass;
		cgi_extension = other.cgi_extension;
		upload_store = other.upload_store;
		redirect = other.redirect;
		max_body_size = other.max_body_size;
	}
	return *this;
}
LocationConfig::~LocationConfig() {}

void LocationConfig::reset()
{
	path.clear();
	root.clear();
	methods.clear();
	autoindex = false;
	index.clear();
	cgi_pass.clear();
	cgi_extension.clear();
	upload_store.clear();
	redirect.clear();
	max_body_size = 0;
}
