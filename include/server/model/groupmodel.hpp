#ifndef GROUPMODEL_H
#define GROUPMODEL_H
#include "group.hpp"
class GroupModel
{
public:
    bool createGroup(Group& group);
    void addGroup(int groupid,int userid,string grouprole);
    vector<Group> queryGroups(int userid);
    vector<int> queryGroupUsers(int userid,int groupid);
};
#endif