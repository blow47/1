#pragma once
//服务器要处理的业务逻辑

//启动epoll模型
int initListenFd(unsigned short port);
int epollRun(unsigned int port);
//和客户端建立新连接
int acceptConn(int lfd, int epfd);
//接受客户端http请求信息
int recvHttpRequest(int cfd);
//解析请求行
int parseRequestLine(const char* reqLine);
//发送头信息：状态行+消息报文+空行
int sendHeadMsg();
//读文件内容
int sendfile(const char* filename);