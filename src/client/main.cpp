#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <thread>
#include <unordered_map>
#include <semaphore.h>

#include "user.hpp"
#include "group.hpp"
#include "json.hpp"
#include "public.hpp"
using namespace std;
using json=nlohmann::json;

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
//展示当前用户的个人信息
void showCurrentUserData();
//接受线程
void readTaskHandler(int fd);
//获取当前时间
string getCurrentTime();
//聊天主菜单页面
void mainMenu(int clientfd);
//记录用户是否登录
bool isRunning;
//记录读线程的数量
static int readTaskNum=0;
//用于接收线程和主线程的通信
sem_t rwsem;
//判断登录是否成功
bool isLoginSuccess=false;

int main(int argc,char** argv)
{
    if(argc<3)
    {
        cerr<<"参数输入错误"<<endl;
        exit(-1);
    }

    char* ip=argv[1];
    uint16_t port=atoi(argv[2]);

    sockaddr_in server;
    memset(&server,0,sizeof(server));
    server.sin_family=AF_INET;
    server.sin_port=htons(port);
    server.sin_addr.s_addr=inet_addr(ip);

    int clientfd=socket(AF_INET,SOCK_STREAM,0);
    if(connect(clientfd,(sockaddr*)&server,sizeof(server))==-1)
    {
        cerr<<"连接失败"<<endl;
        close(clientfd);
        exit(-1);
    }

    sem_init(&rwsem,0,0);

    //开启接收线程
    if(readTaskNum==0)
    {
        thread readTask(readTaskHandler,clientfd);
        readTask.detach();
        readTaskNum++;
    }

    while(true)
    {
        //显示登陆菜单
        cout<<"==============="<<endl;
        cout<<"1.login"<<endl;
        cout<<"2.register"<<endl;
        cout<<"3.quit"<<endl;
        cout<<"==============="<<endl;
        cout<<"choice:";
        int choice=0;
        cin>>choice;
        cin.get();
        switch(choice)
        {
            case 1: //登陆服务
            {
                int id=0;
                char password[20]={0};
                cout<<"id:";
                cin>>id;
                cin.get();
                cout<<"password:";
                cin.getline(password,sizeof(password));

                json responsejs;
                responsejs["msgid"]=LOGIN_MSG;
                responsejs["id"]=id;
                responsejs["password"]=password;
                string js=responsejs.dump();

                int len=send(clientfd,js.c_str(),strlen(js.c_str())+1,0);
                if(len==-1)
                {
                    cerr<<"send wrong"<<endl;
                }
                else
                {
                    sem_wait(&rwsem);
                    //进入聊天主菜单页面
                    //状态更新
                    if(isLoginSuccess)
                    {
                        isRunning=true; 
                        mainMenu(clientfd);
                    }
                }
                break;
            }
            case 2: //注册服务
            {
                char name[20]={0};
                char password[20]={0};
                cout<<"name:";
                cin.getline(name,sizeof(name));
                cout<<"password:";
                cin.getline(password,sizeof(password));

                json responsejs;
                responsejs["msgid"]=REG_MSG;
                responsejs["name"]=name;
                responsejs["password"]=password;

                string js=responsejs.dump();
                int len=send(clientfd,js.c_str(),strlen(js.c_str())+1,0);
                if(len==-1)
                {
                    cerr<<"send wrong"<<endl;
                }
                else
                {
                    sem_wait(&rwsem);
                }
                break;
            }
            case 3: //退出服务
            {
                close(clientfd);
                sem_destroy(&rwsem);
                exit(0);
            }
            default:
            {
                cerr<<"invalid input"<<endl;
                break;
            }
        }

    }

}

void help(int=1,string=" ");

void chat(int clientfd,string str);

void addfriend(int clientfd,string str);

void creategroup(int clientfd,string str);

void addgroup(int clientfd,string str);

void groupchat(int clientfd,string str);

void loginout(int clientfd,string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}
};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}
};


void showCurrentUserData()
{
    cout<<"============login in============"<<endl;
    cout<<"current login user => id:"<<g_currentUser.getId()<<" name:"<<g_currentUser.getName()<<endl;
    cout<<"-------------friendlists--------------"<<endl;
    if(!g_currentUserFriendList.empty())
    {
        for(auto &user:g_currentUserFriendList)
        {
            cout<<user.getId()<<" "<<user.getName()<<" "<<user.getState()<<endl;
        }
    }

    cout<<"-------------grouplists--------------"<<endl;
    if(!g_currentUserGroupList.empty())
    {
        for(auto &group:g_currentUserGroupList)
        {
            cout<<group.getId()<<" "<<group.getName()<<" "<<group.getDesc()<<endl;
            for(auto &groupuser:group.getUsers())
            {
                cout<<groupuser.getId()<<" "<<groupuser.getName()<<" "<<groupuser.getRole()<<" "<<groupuser.getState()<<endl;
            }
        }
    }
    cout<<"==============================="<<endl;
}

string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

void mainMenu(int clientfd)
{
    help();
    char buffer[1024]={0};

    while(isRunning)
    {
        cin.getline(buffer,1024);
        string commandbuf=buffer;
        string command;
        int idx=commandbuf.find(":");
        if(idx==-1)
        {
            command=commandbuf;
        }
        else
        {
            command=commandbuf.substr(0,idx);
        }
        auto it=commandHandlerMap.find(command);
        if(it==commandHandlerMap.end())
        {
            cout<<"命令错误,请重新输入"<<endl;
            continue;
        }

        it->second(clientfd,commandbuf.substr(idx+1,commandbuf.size()-idx));

    }
}

void help(int,string)
{
    cout<<"show command list >>"<<endl;
    for(auto &it:commandMap)
    {
        cout<<it.first<<' '<<it.second<<endl;
    }
    cout<<endl;
}

void chat(int clientfd,string str)
{
    int idx=str.find(":");
    if(idx==-1)
    {
        cout<<"命令错误"<<endl;
        return ;
    }

    int friendid=atoi(str.substr(0,idx).c_str());
    string msg=str.substr(idx+1,str.size()-idx-1);

    json js;
    js["msgid"]=ONE_CHAT_MSG;
    js["toid"]=friendid;
    js["msg"]=msg;
    js["name"]=g_currentUser.getName();
    js["id"]=g_currentUser.getId();
    js["time"]=getCurrentTime();    

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"聊天js发送失败"<<endl;
    }

}

void addfriend(int clientfd,string str)
{
    int friendid=atoi(str.c_str());
    json js;
    js["friendid"]=friendid;
    js["msgid"]=ADD_FRIEND_MSG;
    js["id"]=g_currentUser.getId();

    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"添加好友请求发送失败"<<endl;
    }
}

void creategroup(int clientfd,string str)
{
    int idx=str.find(":");
    if(idx==-1)
    {
        cout<<"命令错误"<<endl;
        return;
    }
    string name=str.substr(0,idx);
    string desc=str.substr(idx+1,str.size()-idx-1);

    json js;
    js["msgid"]=CREATE_GROUP_MSG;
    js["name"]=name;
    js["desc"]=desc;
    js["userid"]=g_currentUser.getId();

    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"creategroup发送失败"<<endl;
    }
}

void addgroup(int clientfd,string str)
{
    int groupid=atoi(str.c_str());
    json js;
    js["msgid"]=ADD_GROUP_MSG;
    js["groupid"]=groupid;
    js["userid"]=g_currentUser.getId();

    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);

    if(len==-1)
    {
        cerr<<"addgroup发送失败"<<endl;
    }
}

void groupchat(int clientfd,string str)
{
    int idx=str.find(":");
    if(idx==-1)
    {
        cerr<<"命令输入有误"<<endl;
        return ;
    }
    int groupid=atoi(str.substr(0,idx).c_str());
    string message=str.substr(idx+1,str.size()-idx);
    
    json js;
    js["msgid"]=GROUP_CHAT_MSG;
    js["groupid"]=groupid;
    js["message"]=message;
    js["userid"]=g_currentUser.getId();
    js["time"]=getCurrentTime();
    js["name"]=g_currentUser.getName();
    string buffer=js.dump();

    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"groupchat js 发送失败"<<endl;
    }
}

void loginout(int clientfd,string)
{
    int userid=g_currentUser.getId();
    json js;
    js["msgid"]=LOGINOUT_MSG;
    js["userid"]=userid;
    
    string buffer=js.dump();
    int len=send(clientfd,buffer.c_str(),strlen(buffer.c_str())+1,0);
    if(len==-1)
    {
        cerr<<"loginout json发送失败"<<endl;
        return;
    }
    isRunning=false;
    g_currentUserFriendList.clear();
    g_currentUserGroupList.clear();
    isLoginSuccess=false;
}

void doLoginResponse(json recv_js)
{
    if(recv_js["errno"].get<int>()==0)  //登录成功
    {
        g_currentUser.setId(recv_js["id"].get<int>());
        g_currentUser.setName(recv_js["name"]); 
        
        //将当前用户的好友列表信息存储到客户端容器
        if(recv_js.contains("friends"))
        {
            vector<string> friend_vec=recv_js["friends"];
            for(auto &f:friend_vec)
            {
                json friend_js=json::parse(f);
                User user;
                user.setId(friend_js["id"].get<int>());
                user.setName(friend_js["name"]);
                user.setState(friend_js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        //将当前用户的群组列表信息存储到客户端容器
        if(recv_js.contains("groups"))
        {
            vector<string> groups_vec=recv_js["groups"];
            for(auto &g:groups_vec)
            {
                json groups_js=json::parse(g);
                Group group;
                group.setId(groups_js["id"].get<int>());
                group.setName(groups_js["name"]);
                group.setDesc(groups_js["desc"]);
                vector<string> users_vec=groups_js["users"];
                for(string &users:users_vec)
                {
                    json js=json::parse(users);
                    GroupUser groupuser;
                    groupuser.setId(js["id"].get<int>());
                    groupuser.setName(js["name"]);
                    groupuser.setState(js["state"]);
                    groupuser.setRole(js["role"]);
                    group.getUsers().push_back(groupuser);
                }
                g_currentUserGroupList.push_back(group);
            }
        }

        //显示用户登录信息
        showCurrentUserData();
        
        //显示当前离线消息，个人聊天信息或者群组消息
        if(recv_js.contains("offlinemsg"))
        {
            vector<string> vec=recv_js["offlinemsg"];
            for(auto &str:vec)
            {
                //time+id+name+"said"+message
                json js=json::parse(str);

                if(js["msgid"].get<int>()==ONE_CHAT_MSG)
                {
                    cout<<js["time"].get<string>()<<" ["<<js["id"]<<"]"<<
                    js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
                }
                else
                {
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["userid"] << "]" 
                    << js["name"].get<string>()<< " said: " << js["message"].get<string>() << endl;
                } 
            }
            cout<<"==============================="<<endl;
        }
        isLoginSuccess=true;
    }
    else
    {
        cerr<<recv_js["errmsg"]<<endl;
    }
}

void doRegResponse(json js)
{
    if(js["errno"].get<int>()==0)
    {
        cout<<"注册成功，id为"<<js["id"]<<endl;
    }
    else
    {
        cout<<"注册失败，用户已经存在"<<endl;
    }
}

void readTaskHandler(int clientfd)
{
    while(true)
    {
        char buffer[1024]={0};
        int len=recv(clientfd,buffer,1024,0);
        if(len==0||len==-1)
        {
            close(clientfd);
            exit(-1);
        }

        //反序列化
        json js=json::parse(buffer);

        //成功接受个人消息
        if(js["msgid"].get<int>()==ONE_CHAT_MSG)
        {
            cout<<js["time"].get<string>()<<" ["<<js["id"]<<"]"<<
            js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
            continue;
        }

        //成功接收群消息
        if(js["msgid"].get<int>()==GROUP_CHAT_MSG)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["userid"] << "]" 
            << js["name"].get<string>()<< " said: " << js["message"].get<string>() << endl;
            continue;
        }

        //成功接收登录消息
        if(js["msgid"].get<int>()==LOGIN_MSG_ACK)
        {
            doLoginResponse(js);
            sem_post(&rwsem);
            continue;
        }

        //成功接收注册消息
        if(js["msgid"].get<int>()==REG_MSG_ACK)
        {
            doRegResponse(js);
            sem_post(&rwsem);
            continue;
        }
    }
}
