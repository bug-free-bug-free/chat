#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <iostream>
using namespace std;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginOut, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把online状态的用户，设置成offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp) {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务  id  pwd   pwd
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }
            
            // id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id); 
            
            // 登录成功，更新用户状态信息 state offline=>online
            user.setState("online");
            _userModel.updateState(user);
            
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息
            vector<string> s_vec = _offlineMsgModel.query(id);
            if (!s_vec.empty())
            {
                response["offlinemsg"] = s_vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            //查询他的好友信息
            vector<User> u_vec=_friendModel.query(id);
            if(!u_vec.empty())
            {
                vector<string> vec;
                for(User &u:u_vec)
                {
                    json j;
                    j["id"]=u.getId();
                    j["name"]=u.getName();
                    j["state"]=u.getState();
                    vec.push_back(j.dump());
                }
                response["friends"]=vec;
            }
            //查询群组信息

            vector<Group> groupuser_vec=_groupModel.queryGroups(id);
            if(!groupuser_vec.empty())
            {
                vector<string> g_vec;
                for(auto &group:groupuser_vec)
                {
                    json gjs;
                    gjs["id"]=group.getId();
                    gjs["name"]=group.getName();
                    gjs["desc"]=group.getDesc();
                    vector<string> u_vec;
                    for(auto &groupuser:group.getUsers())
                    {
                        
                        json ujs;
                        ujs["id"]=groupuser.getId();
                        ujs["name"]=groupuser.getName();
                        ujs["state"]=groupuser.getState();
                        ujs["role"]=groupuser.getRole();
                        u_vec.push_back(ujs.dump());
                    }
                    gjs["users"]=u_vec;
                    g_vec.push_back(gjs.dump());
                }
                response["groups"]=g_vec;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        // 该用户不存在，用户存在但是密码错误，登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }

    
}

// 处理注册业务  name  password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
    
}


// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
   
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it=_userConnMap.begin();it!=_userConnMap.end();it++)
        {
            if(it->second==conn)
            {
                // 从map表删除用户的链接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    //更新用户的状态信息
    if(user.getId()!=-1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//处理客户注销服务
void ChatService::loginOut(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
   
    int userid=js["userid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(userid);
        if(it==_userConnMap.end())
        {
            return ;
        }
        _userConnMap.erase(it);
    }
    //用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user(userid,"","","offline");
    _userModel.updateState(user);
    
}

//添加朋友
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();
    _friendModel.insert(id,friendid);
}

//用户点对点聊天
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid=js["toid"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it=_userConnMap.find(toid);
        if(it!=_userConnMap.end())
        {
            // toid在线，转发消息   服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return ;
        }
    }

    //查询toid是否在线
    User user=_userModel.query(toid);
    if(user.getState()=="online")
    {
        _redis.publish(toid,js.dump());
        return ;
    }

    //toid不在线，存储到对应的离线消息上
    _offlineMsgModel.insert(toid,js.dump());
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid=js["userid"];
    string name=js["name"];
    string desc=js["desc"];
    Group group;
    group.setName(name.c_str());
    group.setDesc(desc.c_str());
    if(_groupModel.createGroup(group))
    {
        _groupModel.addGroup(group.getId(),userid,"creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int groupid=js["groupid"];
    int userid=js["userid"];
    _groupModel.addGroup(groupid,userid,"normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int groupid=js["groupid"];
    int userid=js["userid"];
    string message=js["message"];
    vector<int> vec=_groupModel.queryGroupUsers(userid,groupid);

    lock_guard<mutex> lock(_connMutex);
    // _userConnMap
    for(int id:vec)
    {
        auto it=_userConnMap.find(id);
        if(it!=_userConnMap.end())
        {
            //表示用户在线
            it->second->send(js.dump());
        }
        else
        {
            //查询toid是否在线
            User user=_userModel.query(id);
            if(user.getState()=="online")
            {
                _redis.publish(id,js.dump());
            }
            else
            {
                //表示用户不在线
                _offlineMsgModel.insert(id,js.dump());
            }  
        }
    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}
