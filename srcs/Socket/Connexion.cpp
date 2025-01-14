#include <Socket.hpp>
#include <Ressource.hpp>
#include <Utils.hpp>
#include <Route.hpp>
#include <Router.hpp>

inline bool ends_with(std::string const &value, std::string const &ending)
{
	if (ending.size() > value.size())
		return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

// CONSTRUCTORS & DESTRUCTOR

Connexion::Connexion(const t_network_address netAddr,
					 const t_fd socket, const Router &router, const std::string client_ip_addr) : client_ip_addr(client_ip_addr),
					 																			  c_socket(socket),
																								  netAddr(netAddr),
																								  router(router),
																								  request(t_http_message()),
																								  is_header_parsed(false),
																								  is_request_line_parsed(false),
																								  is_body_parsed(false),
																								  header_read_size(0),
																								  body_read_size(0),
																								  dechunker(request.body, body_read_size),
																								  route(NULL),
																								  ressource(NULL),
																								  resp_start(false),
																								  resp_end(false)
{
}

/**
 * @brief Destroy the Connexion object and call the destructor of the ressource if it exists
 *
 */
Connexion::~Connexion()
{
	Logger::debug << client_ip_addr << " - Connexion is delete" << std::endl;
	if (poll_util(POLL_CTL_DEL, c_socket, this, POLLIN))
		throw std::runtime_error("poll_del failed");
	close(c_socket);
	if (ressource)
		delete ressource;
}

// IOEvent

/**
 * @brief function called when the socket is ready to be read (notify by poll())
 *
 * @return IOEvent
 */
IOEvent Connexion::read()
{
	ssize_t recv_size = recv(c_socket, buffer, BUFFER_SIZE, MSG_DONTWAIT);

	if (recv_size == -1)
		return IOEvent(FAIL, this, client_ip_addr + " - error happened while reading socket");
	if (!recv_size)
		return closed();
	if (is_body_parsed)
		return setError("allready received an entire request", 400);
	raw_request.append(buffer, recv_size);
	if (!is_header_parsed)
		return readHeader();
	return readBody();
}

/**
 * @brief function called when the socket is ready to be written (notify by poll())
 *
 * @return IOEvent
 */
IOEvent Connexion::write()
{
	if (resp_end && response.empty())
		return IOEvent(SUCCESS, this, client_ip_addr + " - successfuly send response");
	if (response.empty())
		return IOEvent();
	Logger::debug << "write to conn" << std::endl;
	if (send(c_socket, response.front().c_str(), response.front().size(), MSG_DONTWAIT) == -1)
		return IOEvent(FAIL, this, client_ip_addr + " - unable to write to the client socket");
	response.pop();
	resp_start = true;
	return IOEvent();
}

/**
 * @brief function called when the socket is closed (notify by poll())
 *
 * @return IOEvent
 */
IOEvent Connexion::closed() { return IOEvent(FAIL, this, client_ip_addr + " - client closed the connexion"); }


/**
 * @brief return the parsed request received by the Connexion object
 *
 * @return t_http_message const&
 */
t_http_message const &Connexion::getRequest() const { return request; }


/**
 * @brief push response chunks to the response queue
 *
 * @param message std::string to push
 */
void Connexion::pushResponse(std::string message) { response.push(message); }


/**
 * @brief push n bytes of response chunks to the response queue
 *
 * @param message std::string to push
 * @param n size_t size of the string
 */
void Connexion::pushResponse(const char *message, size_t n) { response.push(std::string(message, n)); }


/**
 * @brief set the response end flag to true, used to know when the response is fully available in the queue
 *
 */
void Connexion::setRespEnd() { resp_end = true; }


/**
 * @brief return the response end flag
 *
 * @return true
 * @return false
 */
bool Connexion::getRespEnd() const { return resp_end; }

/**
 * @brief return true if the body is fully available in the request.body queue
 *
 * @return true
 * @return false
 */
bool Connexion::getBodyParsed() const { return is_body_parsed; }


//
//		Underlying operations
//

/**
 * @brief helper function to set a response error available to send to the client and log a message to STDERR
 *
 * @param log message to log
 * @param http_error status_code to send to the client
 * @return IOEvent
 */
IOEvent Connexion::setError(std::string log, uint http_error)
{
	std::string error_path;

	if (resp_start)
		return IOEvent(FAIL, this, client_ip_addr + " - " + log);
	response = std::queue<std::string>();
	Logger::warning << client_ip_addr << " - " << http_error << " " << log << std::endl;
	if (route) {
		t_errors_map::const_iterator it = route->getAttributes().error_files.find(http_error);
		if (it != route->getAttributes().error_files.end())
			error_path = it->second;
	}
	if (ressource)
	{
		delete ressource;
		ressource = NULL;
	}
	if (poll_util(POLL_CTL_MOD, c_socket, this, POLLOUT))
		return IOEvent(FAIL, this, client_ip_addr + " - " + "unable to poll_ctl_mod");
	try
	{
		ressource = new GetError(this, http_error, error_path);
	}
	catch (const std::runtime_error &e)
	{
		ressource = new GetError(this, http_error, "");
	}

	resp_end = true;
	return IOEvent();
}

/**
 * @brief split header into string vector and call parseHeader if some lines are ready to be processed
 *
 * @return IOEvent
 */
IOEvent Connexion::readHeader()
{
	size_t pos = 0;
	std::vector<std::string> lines;

	while (pos != std::string::npos && !is_header_parsed)
	{
		if (header_read_size >= MAX_HEADER_SIZE)
			return setError("header exceeds max header size", 413);
		pos = raw_request.find(CRLF);
		lines.push_back(raw_request.substr(0, pos));
		raw_request.erase(0, pos + 2);
		header_read_size += pos;
		if (!pos)
			is_header_parsed = true;
	}
	if (lines.size() > 0)
		return parseHeader(lines);
	return IOEvent();
}

/**
 * @brief use the string vector and save it into t_http_message request
 *
 * @param lines lines splitted from raw_request
 * @return IOEvent
 */
IOEvent Connexion::parseHeader(std::vector<std::string> &lines)
{
	std::vector<std::string>::iterator it = lines.begin();
	size_t pos = 0;
	std::string key;
	std::string value;

	if (!is_request_line_parsed && parseRequestLine(*it++)) // parsing of request line
		return setError("request_line is invalid", 400);
	for (; it != lines.end(); it++)
	{ // parsing of all header fields
		if (it->length() == 0)
			return executeRoute();
		if ((pos = (*it).find(":")) == std::string::npos)
			return setError("header fields not RFC conform", 400);
		key = it->substr(0, pos);
		value = it->substr(it->find_first_not_of(' ', pos + 1));
		pos = value.find_last_not_of(' ');
		if (pos != std::string::npos)
			value.erase(pos + 1);
		if (key.empty() || value.empty() || key.find(' ') != std::string::npos)
			return setError("header fields not RFC conform", 400);
		if (request.header_fields.find(key) != request.header_fields.end())
			request.header_fields[key].append(",");
		request.header_fields[key].append(value);
	}
	return IOEvent();
}

/**
 * @brief parse the request line and save it into t_http_message request
 *
 * @param raw_line std::string representing the raw request line
 * @return true
 * @return false
 */
bool Connexion::parseRequestLine(std::string &raw_line)
{
	std::vector<std::string> splitted_line;
	size_t pos = 0;
	while (pos != std::string::npos)
	{
		pos = raw_line.find(" ");
		splitted_line.push_back(raw_line.substr(0, pos));
		raw_line.erase(0, pos + 1);
	}
	if (splitted_line.size() != 3 || splitted_line[2] != HTTP_VERSION)
		return NOK;
	std::string methodString = splitted_line[0];
	request.request_line.methodVerbose = methodString;
	try{
		request.request_line.method = methodToEnum(methodString);
	}
	catch(std::exception& e){
		return NOK;
	}
	if (decodePercent(splitted_line[1]))
		return NOK;
	request.request_line.path = splitted_line[1];
	request.request_line.http_version = splitted_line[2];
	is_request_line_parsed = true;
	return OK;
}

/**
 * @brief called when the header is parsed,
 * it will retreive the matching route to the connexion and call the route->setRessource
 * if the request contains a body, it will call readBody
 *
 * @return IOEvent
 */
IOEvent Connexion::executeRoute()
{
	// if we are not waiting for a body
	if (request.request_line.methodVerbose == "GET" ||
		(request.header_fields["Transfer-Encoding"].empty() && request.header_fields["Content-Length"].empty()))
	{
		if (body_read_size > 0)
			return setError("request contains body but GET Method, no Transfer-Encoding or Content-Length given", 400);
		if (poll_util(POLL_CTL_MOD, c_socket, this, POLLIN | POLLOUT))
			return setError("unable to set POLL_CTL_MOD", 500);
		is_body_parsed = true;
	}
	else
	{
		if (!request.header_fields["Content-Length"].empty())
		{
			std::stringstream content_length_converter;
			content_length_converter << request.header_fields["Content-Length"];
			if (!(content_length_converter >> request.content_length))
				return setError("Content-Length header field is not correct", 400);
			if (!request.content_length && poll_util(POLL_CTL_MOD, c_socket, this, POLLIN | POLLOUT))
				return setError("unable to set POLL_CTL_MOD", 500);
		}
	}

	if (request.header_fields["Host"].empty())
		return setError("missing Host header field", 400);

	// SETTING UP ROUTE AND RESSOURCE
	route = router.getRoute(netAddr, request);
	if (!route)
		return setError("internal error - route not found", 500);

	IOEvent ioevent_handler = route->setRessource(request, this);
	if (ioevent_handler.result == FAIL)
		return setError(ioevent_handler.log, ioevent_handler.http_error); // setRessource retourne un setError aussi --> redondance
	if (request.content_length > route->getMaxBodySize())
		return setError("Content-Length header field is bigger than the maximum body size allowed for this route", 413);
	// route->handle();

	// if we already have a body in raw_request
	if (!raw_request.empty())
		return readBody();
	return IOEvent();
}

/**
 * @brief get the body of the request and save it into the request.body queue
 *
 * @return IOEvent
 */
IOEvent Connexion::readBody()
{
	// if chunked request
	if (request.header_fields["Transfer-Encoding"] != "" && request.header_fields["Transfer-Encoding"] != "identity")
	{
		IOEvent io_resp = dechunker(raw_request);
		if (io_resp.result == FAIL)
			return setError(io_resp.log, io_resp.http_error);
		else if (io_resp.result == SUCCESS)
		{
			if (poll_util(POLL_CTL_MOD, c_socket, this, POLLIN | POLLOUT))
				return setError("unable to set POLL_CTL_MOD", 500);
			is_body_parsed = true;
		}
	}
	else
	{
		request.body.push(std::string(raw_request, 0, request.content_length - request.body.size()));
		body_read_size += request.body.back().size();
		raw_request.clear();
		if (body_read_size == request.content_length)
		{
			if (poll_util(POLL_CTL_MOD, c_socket, this, POLLIN | POLLOUT))
				return setError("unable to set POLL_CTL_MOD", 500);
			is_body_parsed = true;
		}
	}
	return IOEvent();
}

void Connexion::setRessource(Ressource *_ressource)
{
	ressource = _ressource;
}

/**
 * @brief decode the percent encoding in the uri
 *
 * @param uri std::string representing the uri
 * @return true
 * @return false
 */
bool Connexion::decodePercent(std::string &uri)
{
	std::string::size_type pos = 0;
	while ((pos = uri.find('%', pos)) != std::string::npos)
	{
		if (pos + 2 < uri.length())
		{
			const char hex[3] = {uri[pos + 1], uri[pos + 2], '\0'};
			const char decoded = static_cast<char>(std::strtol(hex, NULL, 16));
			uri.replace(pos, 3, 1, decoded);
			pos += 1;
		}
		else
			return NOK;
	}
	return OK;
}
