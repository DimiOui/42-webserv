#include <Router.hpp>
#include <Ressource.hpp>
#include "Type.hpp"

class CGI;

Route::Route(const t_attributes attributes)
	: attributes(attributes)
{
}
Route::Route(const Route &from)
	: attributes(from.attributes) {}
Route &Route::operator=(const Route &rhs)
{
	// self-assignment guard
	if (this == &rhs)
		return *this;
	attributes = rhs.attributes;
	return *this;
}
Route::~Route()
{
}

size_t Route::getMaxBodySize() const
{
	return attributes.max_body_length;
}
void Route::printAttributes() const
{
	std::cout << LISTEN << ": \t\t" << attributes.port << std::endl;
	std::cout << SERVERNAMES << ": \t\t";
	std::vector<std::string>::const_iterator str_iter;
	for (str_iter = attributes.server_name.begin(); str_iter != attributes.server_name.end(); ++str_iter)
		std::cout << *str_iter << " ";
	std::cout << std::endl;
	std::cout << "Location: \t\t" << attributes.location << std::endl;
	std::cout << ALLOWEDMETHODS << ": \t";
	if (attributes.allowed_methods & GET)
		std::cout << "GET ";
	if (attributes.allowed_methods & POST)
		std::cout << "POST ";
	if (attributes.allowed_methods & DELETE)
		std::cout << "DELETE";
	std::cout << std::endl;
	std::cout << MAXBODYSIZE << ": \t" << attributes.max_body_length << std::endl;
	std::cout << REDIRECT << ": \t" << attributes.redirect << std::endl;
	std::cout << ROOT << ": \t\t\t" << attributes.root << std::endl;
	std::cout << INDEX << ": \t\t\t" << attributes.index << std::endl;
	std::cout << AUTOINDEX << ": \t\t" << (attributes.directory_listing ? "on" : "off") << std::endl;
	std::cout << CGISETUP << ": \t\t";
	for (std::map<std::string, std::string>::const_iterator it = attributes.cgi_path.begin(); it != attributes.cgi_path.end(); ++it)
	{
		std::cout << it->first << "->" << it->second << " ";
	}
	std::cout << std::endl;
	std::cout << UPLOADS << ": \t\t" << attributes.uploadsFolder << std::endl;
	std::cout << ERRORFILE << ": \t\t" << attributes.error_files << std::endl;
}

IOEvent Route::checkRequest(const t_http_message &req, Connexion *conn) const
{
	t_methods request_method = req.request_line.method;
	if (attributes.allowed_methods & request_method)
	{
		// je sais pas trop comment utiliser le debugge ici
		return conn->setError("", 400);
		// return 1;
	}
	return IOEvent();
}

int Route::isCGI(std::string const &path) const
{
	std::map<std::string, std::string>::const_iterator cgiIt;
	int i = 0;
	for (cgiIt = attributes.cgi_path.begin(); cgiIt != attributes.cgi_path.end(); cgiIt++)
	{
		if (containsSubstring(path, cgiIt->first))
		{
			return (i);
		}
		i++;
	}
	return (-1);
}

std::string findCgiExecPath(std::map<std::string, std::string> const &cgiPathMap, int cgiIndex)
{
	std::map<std::string, std::string>::const_iterator cgiMapIter = cgiPathMap.begin();
	// iterating through the CGI paths map in order to find the right path of our executanle;
	for (int i = 0; i != cgiIndex; i++)
	{
		cgiMapIter++;
	}
	return (cgiMapIter->second);
}

IOEvent Route::setCGI(Connexion *conn, int cgi_index, const std::string script_path) const
{
	const std::string &execPath = findCgiExecPath(attributes.cgi_path, cgi_index);
	if (checkPermissions(script_path, R_OK) && checkPermissions(execPath, X_OK))
	{
		const t_cgiInfo cgiInfo(
			extractBeforeChar(script_path, '?'),
			extractAfterChar(script_path, '?'),
			execPath);
		try
		{
			conn->setRessource(new CGI(conn, cgiInfo));
			return (IOEvent());
		}
		catch (const IOExcept &e)
		{
			return conn->setError(e.IOwhat().log, e.IOwhat().http_error);
		}
	}
	else
		return conn->setError("", 404);
}

IOEvent Route::setRessource(const t_http_message &req, Connexion *conn) const
{
	t_request_line reqLine = req.request_line;
	std::string sub_path = reqLine.path.substr(attributes.location.size());
	if (*attributes.location.rbegin() == '/')
		sub_path += "/";
	std::string completePath = attributes.root + sub_path;

	// Logger::debug << completePath << std::endl;

	// 1- redirect-handling
	if (!attributes.redirect.empty())
	{
		conn->setRessource(new RedirectRessource(conn, attributes.redirect));
		return (IOEvent());
	}

	// 2- CGI handling
	int cgiIndex = isCGI(completePath);
	if (cgiIndex >= 0)
		return setCGI(conn, cgiIndex, completePath);

	// 3- Directory handling
	if (directoryExists(completePath.c_str()))
	{
		const std::string indexPath = completePath + attributes.index;
		if ((cgiIndex = isCGI(indexPath)) >= 0)
			return (setCGI(conn, cgiIndex, indexPath));
		if (reqLine.method == GET && (attributes.allowed_methods & GET))
		{
			if (fileExists(indexPath.c_str()))
			{
				if (checkPermissions(indexPath, R_OK))
				{
					if (!(attributes.allowed_methods & GET))
						return IOEvent(FAIL, conn, "", 405);
					try
					{
						conn->setRessource(new GetStaticFile(conn, indexPath));
						return (IOEvent());
					}
					catch (const IOExcept &e)
					{
						return conn->setError(e.IOwhat().log, e.IOwhat().http_error);
					}
				}
				else
					return conn->setError("", 403);
			}
			else if (attributes.directory_listing && checkPermissions(completePath, X_OK))
			{
				try
				{
					conn->setRessource(new GetDirectory(conn, completePath));
					return (IOEvent());
				}
				catch (const IOExcept &e)
				{
					return conn->setError(e.IOwhat().log, e.IOwhat().http_error);
				}
			}
			else
			{
				return IOEvent(FAIL, conn, "", 403);
			}
		}
		else
			return conn->setError("", 405);
	}

	// 4- File handling

	// 4-1 POST case
	if (reqLine.method == POST)
	{
		if (!(attributes.allowed_methods & POST))
			return IOEvent(FAIL, conn, "", 405);
		// if (directoryExists(reqLine.path.c_str()))
		// 	return conn->setError("", 403);
		std::string uploadFolderPath = attributes.root + attributes.uploadsFolder;
		if (!directoryExists(uploadFolderPath.c_str()))
		{
			try
			{
				createFolder(uploadFolderPath);
			}
			catch (const std::runtime_error &e)
			{
				return conn->setError(e.what(), 500);
			}
		}
		std::string completeUploadPath = uploadFolderPath + sub_path;
		try
		{
			conn->setRessource(new PostStaticFile(conn, completeUploadPath));
		}
		catch (const IOExcept &e)
		{
			return conn->setError(e.IOwhat().log, e.IOwhat().http_error);
		}

		return (IOEvent());
	}
	// 4-2 GET, DELETE case
	else if (fileExists(completePath.c_str()))
	{
		try
		{
			switch (reqLine.method)
			{
			case GET:
				if (!(attributes.allowed_methods & GET))
					return IOEvent(FAIL, conn, "", 405);
				conn->setRessource(new GetStaticFile(conn, completePath));
				break;
			case DELETE:
				if (!(attributes.allowed_methods & DELETE))
					return IOEvent(FAIL, conn, "", 405);
				conn->setRessource(new DeleteStaticFile(conn, completePath));
				break;
			default:
				return conn->setError("", 405);
			}
		}
		catch (const IOExcept &e)
		{
			return conn->setError(e.IOwhat().log, e.IOwhat().http_error);
		}
		return (IOEvent());
	}
	// 5- other
	return conn->setError("", 404);
}
