#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H
#include "db.h"
#include "user.hpp"
#include <vector>
class FriendModel
{
    public:
    //添加好友
    void insert(int userid,int friendid);

    //查询好友
    vector<User> query(int id);
};

#endif