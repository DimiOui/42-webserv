#include <Ressource.hpp>

/**************************************************************************/
/******************************** GET FILE ********************************/
/**************************************************************************/

GetStaticFile::GetStaticFile(Connexion *conn, std::string file_path) :	Ressource(conn)
{
	//need to construct the header ?!
	fd_read = open(file_path.c_str(), O_RDONLY | O_NONBLOCK);
	set_nonblocking(fd_read);
}

GetStaticFile::~GetStaticFile()
{
	epoll_util(EPOLL_CTL_DEL, fd_read, this, EPOLLIN);
	close(fd_read);
}

IOEvent	GetStaticFile::read()
{
	size_t	ret;

	ret = recv(fd_read, buffer, BUFFER_SIZE, MSG_DONTWAIT);
	conn->append_response(buffer, ret);
	if (ret == 0)
		is_EOF = true;
	return IOEvent();
}

IOEvent	GetStaticFile::closed()
{
	return IOEvent(FAIL, this, "GetStaticFile::closed() called");
}

/**************************************************************************/
/******************************** POST FILE *******************************/
/**************************************************************************/

PostStaticFile::PostStaticFile(Connexion *conn, std::string file_path) :	Ressource(conn)
{
	fd_write = open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, CH_PERM);
	set_nonblocking(fd_write);
}

PostStaticFile::~PostStaticFile()
{
	epoll_util(EPOLL_CTL_DEL, fd_write, this, EPOLLIN);
	close(fd_write);
}

IOEvent	PostStaticFile::write()
{
	if (send(fd_write, conn->getRequest().body.c_str(),
		conn->getRequest().body.size(), MSG_DONTWAIT) == -1)
		return IOEvent(FAIL, this, "PostStaticFile::write() failed");
}
IOEvent	PostStaticFile::closed()
{
	return IOEvent(FAIL, this, "PostStaticFile::closed() called");
}

/**************************************************************************/
/******************************* DELETE FILE ******************************/
/**************************************************************************/

DeleteStaticFile::DeleteStaticFile(Connexion *conn, std::string file_path) :	Ressource(conn)
{
	int	rm = remove(file_path.c_str());
	if (rm != 0)
		throw std::runtime_error("DeleteStaticFile::DeleteStaticFile() failed");
}

DeleteStaticFile::~DeleteStaticFile() {}
IOEvent	DeleteStaticFile::closed()
{
	return IOEvent(FAIL, this, "DeleteStaticFile::closed() called");
}
