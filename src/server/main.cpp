#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>
using namespace std;

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc,char** argv)
{
    signal(SIGINT, resetHandler);
    if(argc<3)
    {
        cerr<<"参数输入错误"<<endl;
        exit(-1);
    }
    char* ip=argv[1];
    uint16_t port=atoi(argv[2]);

    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();

    return 0;
}