#ifndef __TASK_H__
#define __TASK_H__

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>

#include "Type.h"
#include "fdwrapper.h"

// 只实现了GET
enum METHOD {GET, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH};
enum ANALYZE_STATE {REQUESTLINE_STATE, HEADER_STATE, CONTENT_STATE};
enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};
enum LINK_STATE {OK, BAD, OPEN};

class Task {
public:
	static const int filenameLen = 200;
	static const int readBufferSize = 2048;
	static const int writeBufferSize = 2048;
public:
	Task(){};
	~Task(){};
	void init(int sockfd, const sockaddr_in& addr);
	void closeConnect(bool isClose = true);
	bool read();
	bool write();
	void process();
private:
	void init();
	LINK_STATE analyzeLine();
	HTTP_CODE analyzeRequestLine(char* lineText);
	HTTP_CODE analyzeHeaders(char* lineText);
	HTTP_CODE analyzeContent(char* lineText);
	HTTP_CODE getTargetFile();
	char* getLine() {return readBuffer + startLine;}
	HTTP_CODE processRead();

	void unmap();
	bool addResponse(const char* format, ...);
	bool addContentLength(int len);
	bool addLinger();
	bool addBlankLine();

	bool addStatusLine(int status, const char* title);
	bool addHeaders(int len);
	bool addContent(const char* content);
	bool processWrite(HTTP_CODE code);

public:
	static int _epollfd;
	static int userCount;
private:
	// 客户端socket连接信息
	int _sockfd;
	sockaddr_in _address;

	// 读写缓冲区和一些定位索引声明
	char readBuffer[readBufferSize];
	int readIndex;
	int checkIndex;
	int startLine;
	char writeBuffer[writeBufferSize];
	int writeIndex;

	// 状态机状态声明
	ANALYZE_STATE state;
	METHOD method;

	// HTTP请求解析结果
	char fileName[filenameLen];
	char* url;
	char* version;
	char* host;
	int contentLength;
	bool isLinger;

	// 目标文件映射内存块
	char* __fileAddress;
	struct stat __fileStat;
	struct iovec __iv[2];
	int __ivCount;
};

#endif
