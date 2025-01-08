#include "Responder.hpp"

Responder::Responder() {};
Responder::~Responder() {};

const LocationConfig *Responder::findLocation(const ServerConfig &srv, std::string const &path)
{
	size_t bestMatch = 0;
	const LocationConfig *bestLoc = NULL;

	for (size_t i = 0; i < srv.locations.size(); i++)
	{
		const std::string &locPath = srv.locations[i].path;
		if (path.compare(0, locPath.size(), locPath) == 0)
		{
			// значит path начинается на locPath
			if (locPath.size() > bestMatch)
			{
				bestMatch = locPath.size();
				bestLoc = &(srv.locations[i]);
			}
		}
	}

	return bestLoc;
}