#include "friendmodel.hpp"
using namespace std;


//添加好友
void FriendModel::insert(int userid,int friendid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into friend(userid,friendid) values(%d,%d)",userid,friendid);
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

//查询好友
vector<User> FriendModel::query(int userid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql,"select a.id,a.name,a.state from user as a,friend as b where b.friendid=a.id&&b.userid=%d",userid);

    MySQL mysql;
    vector<User> v;
    if(mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row =mysql_fetch_row(res))!=nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                v.push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return v;
}