#ifndef GROUP_H
#define GROUP_H
#include "groupuser.hpp"
#include <vector>
class Group
{
public:
    void setId(int id){this->id=id;}
    void setName(string name){this->name=name;}
    void setDesc(string desc){this->desc=desc;}

    int getId(){return id;}
    string getName(){return name;}
    string getDesc(){return desc;}
    vector<GroupUser>& getUsers(){return v;}
private:
    int id;
    string name;
    string desc;
    vector<GroupUser> v;
};



#endif