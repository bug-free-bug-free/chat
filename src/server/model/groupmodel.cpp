#include "groupmodel.hpp"
#include "db.h"


bool GroupModel::createGroup(Group& group)
{
    char sql[1024]={0};
    sprintf(sql,"insert into allgroup(groupname,groupdesc) values('%s','%s')",
    group.getName().c_str(),group.getDesc().c_str());
    MySQL mysql;
    if(mysql.connect())
    {
        if(mysql.update(sql))
        {
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
        
    }
    return false;
    
}

void GroupModel::addGroup(int groupid,int userid,string grouprole)
{
    char sql[1024]={0};
    sprintf(sql,"insert into groupuser(groupid,userid,grouprole) values(%d,%d,'%s')",
    groupid,userid,grouprole.c_str());
    MySQL mysql;
    if(mysql.connect())
    {
        mysql.update(sql);
    }
}

vector<Group> GroupModel::queryGroups(int userid)
{
    char sql[1024]={0};
    sprintf(sql,"select a.id,a.groupname,a.groupdesc from allgroup as a,groupuser as b where a.id=b.groupid&&b.userid=%d",
    userid);
    MySQL mysql;
    vector<Group> group_vec;

    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                group_vec.push_back(group);
            }
        }
        mysql_free_result(res);
    }

    for(Group& group:group_vec) //不去除自己
    {
        sprintf(sql,"select a.id,a.name,a.state,b.grouprole from user as a,groupuser as b where b.groupid=%d&&b.userid=a.id",
        group.getId());
        
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                GroupUser groupuser;
                groupuser.setId(atoi(row[0]));
                groupuser.setName(row[1]);
                groupuser.setState(row[2]);
                groupuser.setRole(row[3]);
                group.getUsers().push_back(groupuser);
            }
            mysql_free_result(res);
        }
    }
    return group_vec;
}

vector<int> GroupModel::queryGroupUsers(int userid,int groupid) //去除自己
{
    vector<int> vec;
    char sql[1024]={0};
    sprintf(sql,"select userid from groupuser where groupid=%d&&userid!=%d",groupid,userid);
    MySQL mysql;
    if(mysql.connect())
    {
        MYSQL_RES* res=mysql.query(sql);
        if(res!=nullptr)
        {
            MYSQL_ROW row;
            while((row=mysql_fetch_row(res))!=nullptr)
            {
                vec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
        
    }
    return vec;
}