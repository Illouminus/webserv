#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

struct FileEntry
{
	std::string name;
	bool isDir;
	long long size;
	std::string mtime;
};

struct FileEntryCompare
{
	bool operator()(const FileEntry &a, const FileEntry &b) const
	{
		return a.name < b.name;
	}
};

// dirPath: the full path to directory (for example "www/site1/images")
// reqPath: the path that user asked for (for example "/images/")
std::string makeAutoIndexPage(const std::string &dirPath,
										const std::string &reqPath)
{
	DIR *dir = opendir(dirPath.c_str());
	if (!dir)
	{
		std::ostringstream oss;
		oss << "<html><body><h1>Cannot open directory: "
			 << dirPath << "</h1></body></html>";
		return oss.str();
	}

	std::vector<FileEntry> entries;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == "." || name == "..")
			continue; 
		std::string fullPath = dirPath;
		if (!fullPath.empty() &&
			 fullPath[fullPath.size() - 1] != '/')
		{
			fullPath += "/";
		}
		fullPath += name;

		struct stat st;
		if (stat(fullPath.c_str(), &st) == 0)
		{
			bool isDir = S_ISDIR(st.st_mode);
			long long sz = 0;
			if (!isDir)
				sz = static_cast<long long>(st.st_size);
			char timebuf[64];
			struct tm *lt = localtime(&st.st_mtime);
			strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", lt);
			FileEntry fe;
			fe.name = name;
			fe.isDir = isDir;
			fe.size = sz;
			fe.mtime = timebuf;

			entries.push_back(fe);
		}
		else
		{
			// Can log error here
		}
	}
	closedir(dir);

	std::sort(entries.begin(), entries.end(), FileEntryCompare());

	std::ostringstream oss;
	oss << "<!DOCTYPE html>\n"
		 << "<html>\n"
		 << "<head>\n"
		 << "  <meta charset=\"UTF-8\"/>\n"
		 << "  <title>Index of " << reqPath << "</title>\n"
		 << "  <style>\n"
		 << "    body { font-family: sans-serif; }\n"
		 << "    table { border-collapse: collapse; }\n"
		 << "    th, td { border: 1px solid #ccc; padding: 4px 8px; }\n"
		 << "  </style>\n"
		 << "</head>\n"
		 << "<body>\n"
		 << "  <h1>Index of " << reqPath << "</h1>\n"
		 << "  <table>\n"
		 << "    <tr><th>Name</th><th>Size</th><th>Last Modified</th></tr>\n";

	for (size_t i = 0; i < entries.size(); i++)
	{
		const FileEntry &fe = entries[i];
		std::string href = reqPath;
		if (!href.empty() && href[href.size() - 1] != '/')
			href += "/";
		href += fe.name;
		if (fe.isDir && !fe.name.empty() &&
			 fe.name[fe.name.size() - 1] != '/')
		{
			href += "/";
		}
		std::string icon = fe.isDir ? "[DIR]" : "[FILE]";
		std::string sizeStr;
		if (fe.isDir)
			sizeStr = "-";
		else
		{
			std::ostringstream ssz;
			ssz << fe.size;
			sizeStr = ssz.str();
		}
		oss << "<tr>";
		oss << "<td>" << icon << " "
			 << "<a href=\"" << href << "\">"
			 << fe.name << "</a></td>";
		oss << "<td align=\"right\">" << sizeStr << "</td>";
		oss << "<td>" << fe.mtime << "</td>";
		oss << "</tr>\n";
	}

	oss << "  </table>\n"
		 << "</body>\n</html>\n";

	return oss.str();
}