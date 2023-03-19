#include "fdwrapper.h"

int setnonblocking(int fd) {

	int oldOption = fcntl(fd, F_GETFL);
	int newOption = oldOption | O_NONBLOCK;
	fcntl(fd, F_SETFL, newOption);
	return oldOption;
}

void addfd(int epollfd, int fd, bool oneShot) {

	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if (oneShot) {
		event.events |= EPOLLONESHOT;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

void removefd(int epollfd, int fd) {

	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

void modfd(int epollfd, int fd, int ev) {

	epoll_event event;
	event.data.fd = fd;
	event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}
