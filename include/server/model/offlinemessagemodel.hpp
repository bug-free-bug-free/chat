#ifndef OFFLINEMESSAGEMODEL_H
#define OFFLINEMESSAGEMODEL_H

#include <vector>
#include <string>
using namespace std;
class OfflineMsgModel
{
public:
    // 存储用户的离线消息
    void insert(int userid,string msg);

    // 删除用户的离线消息
    void remove(int userid);

    // 查询用户的离线消息
    vector<string> query(int userid);
};


#endif