#ifndef __FDWRAPPER_H__
#define __FDWRAPPER_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

// 封装一些epoll操作
int setnonblocking(int fd);
void addfd(int epollfd, int fd, bool oneShot);
void removefd(int epollfd, int fd);
void modfd(int epollfd, int fd, int ev);

#endif
