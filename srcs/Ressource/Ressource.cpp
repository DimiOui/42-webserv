#include <Ressource.hpp>

Ressource::Ressource(Connexion *conn): conn(conn), fd_read(-1), fd_write(-1) {}
Ressource::~Ressource() {
	Logger::debug << conn->client_ip_addr << " - Ressource is delete" << std::endl;
}
IOEvent	Ressource::read()
{
	int	ret = ::read(fd_read, buffer, BUFFER_SIZE);

	Logger::debug << "read from ressource" << std::endl;

	if (ret == -1)
		return conn->setError("Error reading the file", 500);
	if (ret == 0) {
		conn->setRespEnd();
		poll_util(POLL_CTL_MOD, fd_read, this, 0);
	}
	if (ret > 0)
		conn->pushResponse(buffer, ret);
	return IOEvent();
}

IOEvent	Ressource::write()
{
	if (conn->getRequest().body.empty())
		return IOEvent();

	Logger::debug << "write to ressource" << std::endl;

	std::string		str = conn->getRequest().body.front();

	int ret = ::write(fd_write, str.c_str(), str.size());

	if (ret <= 0)
		return conn->setError("Error while writing to Ressource", 500);
	if (ret < static_cast<int>(str.size()))
		conn->getRequest().body.front() = str.substr(ret);
	else if (ret == static_cast<int>(str.size())) {
		conn->getRequest().body.pop();
		if (conn->getBodyParsed() == true && conn->getRequest().body.empty()) {
			conn->setRespEnd();
			poll_util(POLL_CTL_MOD, fd_write, this, 0);
		}
	}
	return IOEvent();
}

IOEvent	Ressource::closed()
{
	return conn->setError("CGI::closed() called", 500);
}
