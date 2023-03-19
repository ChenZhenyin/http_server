#include "threadpool.h"
#include "Task.h"
#include "fdwrapper.h"

#define MAX_FD_NUMBER 65536
#define MAX_EVENT_NUMBER 10000

const char* serverBusyTips = "Internal Server Busy!";

void addsig(int sig, void(handler)(int), bool restart = true) {
	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	if (restart) {
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char* argv[]) {

	if (argc <= 2) {
		printf("usage: %s ipAddress portNumber!\n", basename(argv[0]));
		return 1;
	}

	const char* ip = argv[1];
	int port = atoi(argv[2]);

	addsig(SIGPIPE, SIG_IGN);

	g_threadpool = new threadpool;

	Task* users = new Task[MAX_FD_NUMBER];
	assert(users);
	int userCount = 0;

	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);

	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret >= 0);

	ret = listen(listenfd, 5);
	assert(ret >= 0);

	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd != -1);
	addfd(epollfd, listenfd, false);
	Task :: _epollfd = epollfd;

	while (true) {
		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
		if ((number < 0) && (errno != EINTR)) {
			printf("Epoll failure!\n");
			break;
		}

		for (int i = 0; i < number; ++i) {
			int sockfd = events[i].data.fd;
			if (sockfd == listenfd) {
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof(client_address);
				int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
				if (connfd < 0) {
					printf("errno is : %d\n", errno);
					continue;
				}

				if (Task :: userCount >= MAX_FD_NUMBER) {
					printf("%s", serverBusyTips);
					send(connfd, serverBusyTips, strlen(serverBusyTips), 0);
					close(connfd);
					continue;
				}

				users[connfd].init(connfd, client_address);
			} else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				users[sockfd].closeConnect();
			} else if (events[i].events & EPOLLIN) {
				if (users[sockfd].read()) {
					g_threadpool->append(users + sockfd);
				} else {
					users[sockfd].closeConnect();
				}
			} else if (events[i].events & EPOLLOUT) {
				if (!users[sockfd].write()) {
					users[sockfd].closeConnect();
				}
			} else {
				// TODO
			}
		}
	}

	close(epollfd);
	close(listenfd);
	delete []users;
	delete g_threadpool;
	return 0;
}

