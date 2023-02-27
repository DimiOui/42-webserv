#include <Route.hpp>
#include "Router.hpp"
#include "Type.hpp"

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

std::string Route::getError(uint http_error) const
{
	return attributes.error_files.getError(http_error);
}

size_t Route::getMaxBodySize() const
{
	return attributes.max_body_length;
}
void Route::printAttributes() const
{
	std::cout << "-----Port-----" << std::endl;
	std::cout << attributes.port << std::endl;
	std::cout << "-----Methods allowed-----" << std::endl;
	if (attributes.allowed_methods & GET)
	{
		std::cout << "GET ";
	}
	else if (attributes.allowed_methods & POST)
	{
		std::cout << "POST ";
	}
	else if (attributes.allowed_methods & DELETE)
	{
		std::cout << "DELETE";
	}
	std::cout << std::endl;
	std::cout << "-----Server Names-----" << std::endl;
	std::vector<std::string>::const_iterator str_iter;
	for (str_iter = attributes.server_name.begin(); str_iter != attributes.server_name.end(); ++str_iter)
	{
		std::cout << *str_iter << " ";
	}
	std::cout << std::endl;
	std::cout << "-----Location-----" << std::endl;
	std::cout << attributes.location << std::endl;
	std::cout << "-----Max body length-----" << std::endl;
	std::cout << attributes.max_body_length << std::endl;
	std::cout << "-----Redirect-----" << std::endl;
	std::cout << attributes.redirect << std::endl;
	std::cout << "-----Root-----" << std::endl;
	std::cout << attributes.root << std::endl;
	std::cout << "-----Directory listing-----" << std::endl;
	std::cout << attributes.directory_listing << std::endl;
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
	for (cgiIt = attributes.cgiMap.begin(); cgiIt != attributes.cgiMap.end(); cgiIt++)
	{
		if (containsSubstring(path, cgiIt->first))
		{
			return (i);
		}
		i++;
	}
	return (-1);
}

IOEvent Route::setRessource(const t_http_message &req, Connexion *conn) const
{
	t_request_line	reqLine = req.request_line;
	std::string		completePath = attributes.root + reqLine.path;
	
	if (attributes.redirect.length())
	{
		
		conn->setRessource(new RedirectRessource(conn, attributes.redirect + reqLine.path));
		return (IOEvent());
	}
	
	bool cgi = isCGI(completePath);
	if (cgi > -1)
	{
		if (fileExists(extractBeforeChar(completePath, "?")))
		{
			if (check_permissions(completePath, S_IXUSR | S_IXGRP))
			{
				t_cgi_info cgiInfo(extractBeforeChar(completePath, "?"), extractAfterChar(completePath, "?"), cgiMap[cgi]->second);
				conn->setRessource(new CGI(conn, cgiInfo));
				return (IOEvent());
			}
			else
			{
				return (conn->setError("", 403));
			}
		}
	}
	if (fileExists(completePath))
	{
		if (reqLine.method & GET)
		{
			if (checkPermissions(completePath, S_IRUSR | S_IRGRP))
			{
				conn->setRessource(new GetStaticFile(conn, completePath));
				return (IOEvent());
			}
			else
			{
				return (conn->setError("", 403));
			}
		}
		else if (reqLine.method & POST)
		{
			conn->setRessource(new PostStaticFile(conn, completePath))
			return (IOEvent());
		}
	}
	else if (directoryExists(completePath) && reqLine.method & GET)
	{
		if (checkPermissions(completePath, S_IRUSR | S_IRGRP))
		{
			if (fileExists(completePath + "index.html"))
			{
				if (checkPermissions(completePath + "index.html", S_IRUSR | s_IRGRP)) {
					conn->setRessource(new GetStaticFile(conn, completePath));
					return (IOEvent());
				}
			}
			else if (attributes.directory_listing)
			{
				if (checkPermissions(completePath, S_IXUSR | S_IXGRP)) {
					conn->setRessource(new GetDirectory(conn, completePath)); 
					return (IOEvent());
				}
			}
		}
		else
		{
				return (conn->setError("", 403));
		}
	}
}


IOEvent GetStaticFile::read()
{
	size_t ret = ::read(fd_read, buffer, BUFFER_SIZE);

	if (ret == -1)
	{
		close(fd_read);
		return conn->setError("Error reading the file", 500);
	}
	if (!ret)
		return closed();
	if (ret == 0)
		is_EOF = true;
	conn->append_response(buffer, ret);
	return IOEvent();
}