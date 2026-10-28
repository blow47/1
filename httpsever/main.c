#include <stdio.h>

int main(int argc,char* argv[])
{
	// a.out port path(服务器提供的资源根目录)
	if (argc < 3)
	{
		printf("./a.out port respath\n");
		exit(0);
	}
	//资源根目录存储到argv[2]
	//将当前服务器进程工作目录切换到资源根目录
	chdir(argv[2]);
	//启动服务器
	unsigned short port = aoti(argv[1]);
	epollRun(port);
	return 0;
}
