#include "Task.h"

const char* global_title[] = {
	"OK",
	"Bad Request",
	"Forbidden",
	"404 Not Found", 
	"Internal Error",
	"\0"
};

const char* global_form[] = {
	"",
	"Your request has bad syntax or is inherently impossible to satisfy.\n",
	"You do not have permission to get file from this server.\n",
	"The requested file was not found on this server.\n",
	"There was an unusual problem serving the requested file.\n",
	"\0"
};

const char* rootDir = "/var/www/html";

int Task :: _epollfd = -1;
int Task :: userCount = 0;

void Task :: init() {

	readIndex = 0;
	checkIndex = 0;
	startLine = 0;
	writeIndex = 0;

	state = REQUESTLINE_STATE;
	method = GET;

	url = 0;
	version = 0;
	host = 0;
	contentLength = 0;
	isLinger = false;

	memset(readBuffer, '\0', readBufferSize);
	memset(writeBuffer, '\0', writeBufferSize);
	memset(fileName, '\0', filenameLen);
}

void Task :: init(int sockfd, const sockaddr_in& addr) {

	_sockfd = sockfd;
	_address = addr;

	int error = 0;
	socklen_t len = sizeof(error);
	getsockopt(_sockfd, SOL_SOCKET, SO_ERROR, &error, &len);

	addfd(_epollfd, _sockfd, true);
	++userCount;

	init();
}

void Task :: closeConnect(bool isClose) {

	if (isClose && (_sockfd != -1)) {
		_sockfd = -1;
		--userCount;
	}
}

bool Task :: read() {

	if (readIndex >= readBufferSize) {
		return false;
	}

	int readBytes = 0;
	while (true) {
		readBytes = recv(_sockfd, readBuffer + readIndex, readBufferSize - readIndex, 0);
		if (readBytes == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			}
			return false;
		} else if (readBytes == 0) {
			return false;
		}
		readIndex += readBytes;
	}
	return true;
}

bool Task :: write() {

	int hasSend = 0;
	int needSend = writeIndex;
	if (needSend == 0) {
		init();
		modfd(_epollfd, _sockfd, EPOLLIN);
		return true;
	}

	while (true) {
		int writeBytes = writev(_sockfd, __iv, __ivCount);
		if (writeBytes <= -1) {
			if (errno == EAGAIN) {
				modfd(_epollfd, _sockfd, EPOLLOUT);
				return true;
			}
			unmap();
			return false;
		}

		hasSend += writeBytes;
		needSend -= writeBytes;
		if (hasSend >= needSend) {
			unmap();
			if (isLinger) {
				init();
				modfd(_epollfd, _sockfd, EPOLLIN);
				return true;
			} else {
				modfd(_epollfd, _sockfd, EPOLLIN);
				return false;
			}
		}
	}
}

void Task :: process() {

	HTTP_CODE readResult = processRead();
	if (readResult == NO_REQUEST) {
		modfd(_epollfd, _sockfd, EPOLLIN);
		return;
	}

	bool writeResult = processWrite(readResult);
	if (!writeResult) {
		closeConnect();
	}

	modfd(_epollfd, _sockfd, EPOLLOUT);
}

void Task :: unmap() {

	if (__fileAddress) {
		munmap(__fileAddress, __fileStat.st_size);
		__fileAddress = 0;
	}
}

// 获取一个完整行
LINK_STATE Task :: analyzeLine() {

	for (; checkIndex < readIndex; ++checkIndex) {
		char ch = readBuffer[checkIndex];
		if (ch == '\r') {
			if ((checkIndex + 1) == readIndex) {
				return OPEN;
			} else if (readBuffer[checkIndex + 1] == '\n') {
				readBuffer[checkIndex++] = '\0';
				readBuffer[checkIndex++] = '\0';
				return OK;
			}
			return BAD;
		} else if (ch == '\n') {
			if ((checkIndex > 1) && (readBuffer[checkIndex - 1] == '\r')) {
				readBuffer[checkIndex - 1] = '\0';
				readBuffer[checkIndex++] = '\0';
				return OK;
			}
			return BAD;
		}
	}

	return OPEN;
}

HTTP_CODE Task :: analyzeRequestLine(char* lineText) {

	url = strpbrk(lineText, " \t");
	if (!url) {
		return BAD_REQUEST;
	}
	*url++ = '\0';

	char* _method = lineText;
	if (strcasecmp(_method, "GET") == 0) {
		// 仅支持GET方法
		method = GET;
	} else {
		return BAD_REQUEST;
	}

	url += strspn(url, " \t");
	version = strpbrk(url, " \t");
	if (!version) {
		return BAD_REQUEST;
	}
	*version++ = '\0';
	version += strspn(version, " \t");
	if (strcasecmp(version, "HTTP/1.1") != 0) {
		// 仅支持HTTP/1.1
		return BAD_REQUEST;
	}

	if (strncasecmp(url, "http://", 7) == 0) {
		url += 7;
		url = strchr(url, '/');
	}

	if (!url || url[0] != '/') {
		return BAD_REQUEST;
	}

	state = HEADER_STATE;
	return NO_REQUEST;
}

HTTP_CODE Task :: analyzeHeaders(char* lineText) {

	if (lineText[0] == '\0') {
		if (contentLength != 0) {
			state = CONTENT_STATE;
			return NO_REQUEST;
		}
		return GET_REQUEST;
	} else if (strncasecmp(lineText, "Connection:", 11) == 0) {
		lineText += 11;
		lineText += strspn(lineText, " \t");
		if (strcasecmp(lineText, "keep-alive") == 0) {
			isLinger = true;
		}
	} else if (strncasecmp(lineText, "Content-Length:", 15) == 0) {
		lineText += 15;
		lineText += strspn(lineText, " \t");
		contentLength = atol(lineText);
	} else if (strncasecmp(lineText, "Host:", 5) == 0) {
		lineText += 5;
		lineText += strspn(lineText, " \t");
		host = lineText;
	} else {
		printf("Can not analyze this headers : %s\n", lineText);
	}

	return NO_REQUEST;
}

HTTP_CODE Task :: analyzeContent(char* lineText) {

	if (readIndex >= (checkIndex + contentLength)) {
		lineText[contentLength] = '\0';
		return GET_REQUEST;
	}
	return NO_REQUEST;
}

// 映射目标文件到一段内存空间
HTTP_CODE Task :: getTargetFile() {

	strcpy(fileName, rootDir);
	int len = strlen(rootDir);
	strncpy(fileName + len, url, filenameLen - len - 1);
	if (stat(fileName, &__fileStat) < 0) {
		return NO_RESOURCE;
	}

	if (!(__fileStat.st_mode & S_IROTH)) {
		return FORBIDDEN_REQUEST;
	}

	if (S_ISDIR(__fileStat.st_mode)) {
		return BAD_REQUEST;
	}

	int fd = open(fileName, O_RDONLY);
	__fileAddress = (char*)mmap(0, __fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
}

// 解析HTTP请求
HTTP_CODE Task :: processRead() {

	LINK_STATE lineStatus = OK;
	while (((state == CONTENT_STATE) && (lineStatus == OK)) || ((lineStatus = analyzeLine()) == OK)) {
		char* lineText = getLine();
		startLine = checkIndex;
		printf("Get a http line: %s\n", lineText);

		// 状态转移
		switch (state) {
			case REQUESTLINE_STATE : {
				HTTP_CODE result = analyzeRequestLine(lineText);
				if (result == BAD_REQUEST) {
					return BAD_REQUEST;
				}
				break;
			}
			case HEADER_STATE : {
				HTTP_CODE result = analyzeHeaders(lineText);
				if (result == BAD_REQUEST) {
					return BAD_REQUEST;
				} else if (result == GET_REQUEST) {
					return getTargetFile();
				}
				break;
			}
			case CONTENT_STATE : {
				HTTP_CODE result = analyzeContent(lineText);
				if (result == GET_REQUEST) {
					return getTargetFile();
				}
				lineStatus = OPEN;
				break;
			}
			default : {
				return INTERNAL_ERROR;
			}
		}
		
	}

	return NO_REQUEST;
}

bool Task :: addResponse(const char* format, ...) {

	if (writeIndex >= writeBufferSize) {
		return false;
	}

	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(writeBuffer + writeIndex, writeBufferSize - 1 - writeIndex, format , arg_list);
	if (len >= (writeBufferSize - 1 - writeIndex)) {
		return false;
	}
	writeIndex += len;
	va_end(arg_list);

	return true;
}

bool Task :: addContentLength(int len) {

	return addResponse("Content-Length: %d\r\n", len);
}

bool Task :: addLinger() {

	return addResponse("Connection: %s\r\n", (isLinger == true) ? "keep-alive" : "close");
}

bool Task :: addBlankLine() {

	return addResponse("%s", "\r\n");
}

bool Task :: addStatusLine(int status, const char* title) {

	return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool Task :: addHeaders(int len) {

	addContentLength(len);
	addLinger();
	addBlankLine();
	return true;
}

bool Task :: addContent(const char* content) {

	return addResponse("%s", content);
}

// 填充HTTP响应
bool Task :: processWrite(HTTP_CODE result) {

	switch (result) {
		case BAD_REQUEST : {
			addStatusLine(400, global_title[1]);
			addHeaders(strlen(global_form[1]));
			if (!addContent(global_form[1])) {
				return false;
			}
			break;
		}
		case FORBIDDEN_REQUEST : {
			addStatusLine(403, global_title[2]);
			addHeaders(strlen(global_form[2]));
			if (!addContent(global_form[2])) {
				return false;
			}
			break;
		}
		case NO_RESOURCE : {
			addStatusLine(404, global_title[3]);
			addHeaders(strlen(global_form[3]));
			if (!addContent(global_form[3])) {
				return false;
			}
			break;
		}
		case INTERNAL_ERROR : {
			addStatusLine(500, global_title[4]);
			addHeaders(strlen(global_form[4]));
			if (!addContent(global_form[4])) {
				return false;
			}
			break;
		}
		case FILE_REQUEST : {
			addStatusLine(200, global_title[0]);
			if (__fileStat.st_size != 0) {
				addHeaders(__fileStat.st_size);
				__iv[0].iov_base = writeBuffer;
				__iv[0].iov_len = writeIndex;
				__iv[1].iov_base = __fileAddress;
				__iv[1].iov_len = __fileStat.st_size;
				__ivCount = 2;
				return true;
			} else {
				const char* emptyFile = "<html><body></body></html>";
				addHeaders(strlen(emptyFile));
				if (!addContent(emptyFile)) {
					return false;
				}
			}
			
		}
		default : {
			return false;
		}
	}

	__iv[0].iov_base = writeBuffer;
	__iv[0].iov_len = writeIndex;
	__ivCount = 1;
	return true;
}
