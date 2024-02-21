#ifndef GROUPUSER_H
#define GROUPUSER_H
#include "user.hpp"
class GroupUser:public User
{
public:
    string getRole(){return role;}
    void setRole(string role){this->role=role;}
private:
    string role;
};


#endif