#include "net.h"

/*
 * service:  this argument can be either a protocol name or a port number.
 *
 * returns:
 * 	This function always success, if not, the program is terminated.
 */
int
net_listen(char *service)
{
	int              sock;
	int              ret;
	struct addrinfo  *rp;
	struct addrinfo  *result;
	struct addrinfo  hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE;
	hints.ai_protocol = 0;

	ret = getaddrinfo(NULL, service, &hints, &rp);
	if(ret) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}

	for(rp=result; rp; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sock<0)
			continue;

		ret = bind(sock, rp->ai_addr, rp->ai_addrlen);
		if(!ret)
			break;

		if(ret<0 && errno = EPERM)
			net_sys_err("bind");

		close(sock);
	}

	if(!rp) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinf(result)

	/* 128, check the Linux man page for the listen() system call */
	ret = listen(sock, 128);
	if(ret<0)
		net_sys_err("listen");
   return sock;
}

/*
 */
void
net_accept_connections(int sock, int epollfd)
{
	int                sock;
	int                ret;
	struct net_proxy   *conn_info;
	struct epoll_event ev;

	while(1) {
		len = sizeof(addr);
		ret = accept(sock, NULL, NULL);
		if(ret < 0 && ((errno==EWOULDBLOCK) || (errno==EAGAIN)) )
			break;

		if(ret<0)
			continue;

		net_set_nonblock(ret);
		conn_info = malloc(sizeof(*conn_info));

		conn_info->client = ret;
		conn_info->server = NET_SOCKET_CLOSED;
		ev.events         = EPOLLET | EPOLLIN;
		ev.data.ptr       = conn_info;

		ret = epoll_ctrl(epollfd, EPOLL_CTL_ADD, ret, &ev);
		if(!ret)
			continue;

		if(errno==ENOSPC) { /* epoll can't watch more users */
			close(conn_info->client);
			free(conn_info);
		}
		net_sys_err("epoll");
	}
	return;
}

void
net_check_sockets(struct epoll_event *evs, size_t n_events)
{
	int   i;

	for(i=0; i < n ; i++ ) {
		if((evs[i].events & EPOLLERR) || (evs[i].events & EPOLLHUP) ||
		                                 (!(evs[i].events & EPOLLIN)) ) {
			if(evs[i].data.ptr->dataptr)
				free(evs[i].data.ptr->dataptr);

			close(evs[i].data.ptr->client);

			if(evs[i].data.ptr->server != NET_SOCKET_CLOSED)
				close(evs[i].data.ptr->server);

			free(evs[i].data.ptr);
			evs[i].data.ptr = NULL;
		}
	}
}

/*
 * returns:
 * 	-1 on error, and errno is set appropriately.
 * 	otherwise a file descriptor that refers to a connected socket.
 */
int
net_connect(const char *host, const char *service)
{
	int               sock;
	int               ret;
	struct addrinfo   hints;
	struct addrinfo   *result;
	struct addrinfo   *rp;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family     = AF_UNSPEC;
	hints.ai_socktype   = SOCK_STREAM;
	hints.ai_flags      = 0;
	hints.ai_protocol   = 0;

	ret = getaddrinfo(host, service, &hints, &result);
	if(ret<0)
		return -1;

	for(rp = result; rp; rp = rp = rp->ai_next) {
		ret = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(ret<0)
			return -1;

		sock = connect(ret, rp->ai_addr, rp->ai_addrlen)
		if(sock!=-1)
			break;
	}

	freeaddrinfo(result);
   return sock;
}

void
net_set_nonblock(int sock)
{
	int flags;
	int ret;

	ret = fcntl(sock, F_GETFL, 0);
	if(ret<0)
		net_sys_err("fcntl");

	ret = fcntl(sock, F_SETFL, O_NONBLOCK|ret);
	if(ret<0)
		net_sys_err("fcntl");
    return;
}

ssize_t
net_send(int sock, const char *buf, size_t nbytes)
{
	size_t       idx  =0;
	ssize_t      n    =0;
	const size_t ret = nbytes;

	do {
		idx += n;
		n = write(sock, &buf[idx], nbytes);
		if(n < 0)
			return -1;

		nbytes -= n;
	} while(nbytes);
   return ret;
}

static int
net_splice(int in, int out, size_t data_len)
{
	int     len;
	size_t  nbytes     =0;

	do {
		len = splice(in, NULL, out, NULL, data_len,
		SPLICE_F_MOVE | SPLICE_F_NONBLOCK);

		if(len <= 0) {
			if(len<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) {
				return nbytes;
			} else if(!len) {
				return nbytes;
			} else {
				return -1;
			}
		}
		data_len -= len;
		nbytes   += len;
	} while(data_len);
   return nbytes;
}

/*
 * returns:
 *
 */
int
net_exchange(int sock_in, int sock_out, size_t nbytes)
{
	int      ret;
	int      pipefd[2];

	pipe(pipefd);
	if(ret<0) {
		perror("pipe()");
		exit(EXIT_FAILURE);  /* pipes are a very important resource */
	}

	ret = splice_all(sock_in, NULL, pipefd[0], NULL, nbytes);
	if(ret<0)
		return -1;

	ret = splice_all(pipefd[0], NULL, sock_out, NULL, ret);
	if(ret<0)
		return -1;

	close(pipefd[0]);
	close(pipefd[1]);
   return ret;
}

static void
net_sys_err(const char *s)
{
	perror(s);
	exit(EXIT_FAILURE);
}