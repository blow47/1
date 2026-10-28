#include "server.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
int initListenFd(unsigned short port)
{
	//1.创建监听套接字
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1)
	{
		perror("socket");
		return -1;
	}	
	//2.设置端口复用
	int opt = 1;
	int ret = setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	if (ret == -1)
	{
		perror("setsockopt");
		return -1;
	}
	//3.绑定
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(lfd,(struct sockaddr*)&addr,sizeof(addr));
	if (ret == -1)
	{
		perror("bind");
		return -1;
	}
	//4.设置监听
	ret = listen(lfd, 128);
	if (ret == -1)
	{
		perror("listen");
		return -1;
	}
	//5.将得到的可以套接字返回给调用者
	return lfd;
}


int epollRun(unsigned int port)
{
	//1.创建epoll模型
	int epfd = epoll_create(10);
	if (epfd == -1)
	{
		perror("epoll_create");
		return -1;
	}
	//2.初始化epoll模型
	int lfd = initListenFd(port);
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = lfd;
	//添加lfd到检测模型中
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (ret == -1)
	{
		perror("epoll_ctl");
		return -1;
	}
	//检测--循环检测
	struct epoll_event evs[1024];
	int size = sizeof(evs) / sizeof(evs[0]);
	while (1)
	{
		int num = epoll_wait(epfd, evs, size, -1);
		for (int i = 0; i < num; ++i)
		{
			int curfd = evs[i].data.fd;
			if (curfd = lfd)
			{
				//建立新连接
				acceptConn(lfd, epfd);
			}
			else
			{
				//通信 ->先接收数据，然后回复数据
				recvHttpRequest(curfd, epfd);
			}
		}

	}
	return 0;


}

int acceptConn(int lfd,int epfd)
{
	//1.建立新连接
	int cfd = accept(lfd,NULL, NULL);
	if (cfd == -1)
	{
		perror("accept");
		return -1;
	}
	//2.设置通信文件描述符为非阻塞
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);

	//3.通信文件描述符添加到epoll模型中
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;//边沿触发
	ev.data.fd = cfd;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	if (ret == -1)
	{
		perror("epoll_ctl");
		return -1;
	}
	return 0;

}

int recvHttpRequest(int cfd,int epfd)
{
	//因为边沿非阻塞模式，所以循环读数据
	char tmp[1024];//每次接受1k数据
	char buf[4096];//将每次读到的数据存储到buf中

	int len, total=0;//total：当前buf中已经存储了多少数据

	while ((len = recv(cfd, tmp, sizeof(tmp), 0)) > 0)
	{
		if (total + len < sizeof(buf))
		{
			//有空间存储数据
			memcpy(buf + total,tmp,len);
		}
		total += len;

	}

	//循环结束-》读完了
	if (len == -1 && errno == EAGAIN)
	{
		//将请求行从接受数据拿出来
		// 在http协议中换行使用的是\r\n
		// 遍历字符串，当遇到第一个 \r\n的时候意味着请求行拿到了
		char* pt = strstr(buf,"\r\n");
		//请求行字节数（长度）
		int reqlen = pt - buf;
		//保留请求行就可以
		buf[reqlen] = '\0';//字符串截断
		//解析请求行
		parseRequestLine(buf);
	}
	else if(len == 0)
	{
		printf("客户端断开连接。。。\n");
		//服务器断开连接，文件描述符从epoll模型中删除
		disConnect(cfd, epfd);
	}
	else
	{
		perror("recv");
	}

	return 0;

}
//解析请求行
int parseRequestLine(int cfd,const char* reqLine)
{
	//请求行分为三部分
	//GET /hello/world/ http/1.1
	//1.将请求行的三部分依次拆分，有用的前两部分
	//-提交数据方式
	//-客户端向服务器请求的文件名
	char method[6];
	char path[1024];
	sscanf(reqLine,"%[^ ] %[^ ]",method,path);
	//2.判断请求方式是不是get，不是忽略
	if (strcasecmp(method, "get") != 0)
	{
		printf("用户请求方式是get，忽略。。。\n");
		return -1;
	}


	//3.判断用户提交的请求是访问服务器端的文件还是目录
	//需要判断path中存储的是什么字符串
	char* file = NULL;
	if (stcmp(path, "/") == 0)
	{
		//访问服务器提供的资源根目录  假设： /home/robin/aa
		//在启动服务器程序中先指定资源根目录是哪个目录
		//在main函数中将工作目录切换到资源根目录
		file = "./";       //  ./对应的目录就是客户端访问的资源的根目录
		// ./ == /home/robin/aa
	}
	else
	{
		//假设： /hello/a.txt
		//如果不把 / 去掉就相当于访问系统根目录
		file = path + 1; //hello/a.txt == ./hello/a.txt
	}
	//属性判断
	struct stat st;
	int ret = stat(file, &st);
	if (ret == -1)
	{
		//获取文件属性失败 ==》没有这个文件
		//给客户端发送404
		sendHeadMsg(cfd,404,"Not Found",,-1);
		sendfile("404.html");
	}
	if (S_ISDIR(st.st_mode))
	{
		//遍历目录，将目录内容发送给客户端
		sendHeadMsg();

	}
	else
	{
		//如果是普通文件，发送文件内容给客户端
		sendHeadMsg();
		sendfile();
	}


}

int sendfile(const char* filename)
{
	//发送内容前应有 状态行+消息报文+空行+文件内容
	// 不需要组织好再发送
	// 传输层默认使用tcp
	// 面向连接的流式传输协议 -》 只要最后全部发送完就行
	//读文件内容，发送给客户端
	//打开文件
	int fd = open(filename,O_RDONLY);
	while (1)
	{
		char buf[1024] = { 0 };
		int len = read(fd,buf,sizeof(buf));
		if (len > 0)
		{
			send(cfd,buf,len,0);
		}
		else if (len == 0)
		{
			//文件读完
			break;
		}
		else
		{
			perror("文件读取失败。。。。\n");
			return -1;

		}
	}
	return 0;
}
/*
 status:状态码
 descr:状态描述
 type：content-type值
 length:Content-Length
*/
int sendHeadMsg(int cfd,int status,const char* descr,const char* type,int length)
{
	//状态行+消息报文+空行
	char buf[4096];
	// http/1.1 200 ok
	sprintf("http/1.1 %d %s\r\n",status,descr);
	//消息报头 - 》 2个键值对
	//content-type / content-length
	sprintf(buf+strlen(buf),"Content-Type:%s\r\n",type);
	sprintf(buf + strlen(buf), "Content-Length:%d\r\n",length);
	//空行
	//拼接完成之后，发送
	send(cfd,buf,strlen(buf),0);
	return 0;
}
//和客户端断开连接
int disConnect(int cfd, int epfd)
{
	//
	int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
	if (ret == -1)
	{
		perror("epoll_ctl");
		close(cfd);
		return -1;
	}
	close(cfd);
	return 0;
}