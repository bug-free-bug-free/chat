#include "json.hpp"
#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;
using json=nlohmann::json; 

void test1()
{
    json j;
    j["id"]="zhangsan";
    j["name"]="lisi";
    j["message"]="hello world";
    cout<<j<<endl;
}

void test2()
{
    vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    v.push_back(4);

    map<int,string> m;
    m.insert({1,"flfa;"});
    m.insert({1,"wcwe;"});
    m.insert({1,"bbrb;"});
    m.insert({1,"h kh;"});
    json j;
    j["num"]=v;
    j["string"]=m;
    // cout<<j<<endl;
    string str=j.dump();
    cout<<str<<endl;
}

int main()
{
    // test1();
    test2();
    return 0;
}