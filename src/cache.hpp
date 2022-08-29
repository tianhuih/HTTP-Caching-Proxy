#include "Response.hpp"
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

using namespace std;
// this class is for the cache, this would be a LRU cache based on doubly linked list, all info stored in self defined Node of list, there is a std::map help maintain the list
class Cache {
 private:
  //self defined list node
  class LRUNode {
   public:
    std::unordered_map<string, string>
        information;  //Storing all the information about the node
    std::string key; // key: the first line of the request
    Response value; // value, a response object
    LRUNode(const std::string & rkey, const Response & rval) : key(rkey), value(rval){}
    LRUNode(const std::string & rkey, const Response & rval, const std::unordered_map<string, string>& rinformation) : information(rinformation), key(rkey), value(rval) {}
  };

  std::map<std::string, std::list<LRUNode>::iterator> LRUMap;
  std::list<LRUNode> LRUlist;

 public:
  size_t capacity; // cache size
  Cache(size_t rcapacity) : capacity(rcapacity) {}
  
  std::string noModifyGet(const std::string & key) {
    std::unordered_map<std::string,std::string> tempInformation = (*LRUMap[key]).information;
    Response tVal = (*LRUMap[key]).value;
    LRUlist.emplace_back(key, tVal,tempInformation);  // insert to end
    LRUlist.erase(LRUMap[key]);       //remove old place
    std::list<LRUNode>::iterator it = LRUlist.end();
    it--;
    LRUMap[key] = it;  // update Map
    return it->value.getResponseInfo();
  }

  //get the stored infomation of the key
  std::string get(const std::string & key, const double curTime) {
    if (LRUMap.count(key) == 0) {
      return "notfound";
    }
    else if (LRUMap[key]->information["revalidate"] == "true") { // if need validate, return the lines need to be put into the request
      string temp = "revalidate:";
      if (LRUMap[key]->value.getETAG() != "") {
        temp += "If-None-Match: ";
        temp += LRUMap[key]->value.getETAG();
        temp += "\r\n";
      }
      if (LRUMap[key]->value.getLastModified() != "") {
        temp += "If-Modified-Since: ";
        temp += LRUMap[key]->value.getLastModified();
        temp += "\r\n";
      }
      return temp;
    }
    else {
      Response tVal = (*LRUMap[key]).value;
      std::unordered_map<std::string,std::string> tempInformation = (*LRUMap[key]).information;
      if (tVal.getExpireTime() < curTime) {
        std::string tempKey = (*LRUMap[key]).key;
        LRUlist.erase(LRUMap[key]);  //remove old place
        LRUMap.erase(tempKey);
        std::string expireString = "expires:";
        expireString += std::to_string(tVal.getExpireTime()); // get the expire time and return together
        return expireString;
      }
      else {
        LRUlist.emplace_back(key, tVal, tempInformation);  // insert to end
        LRUlist.erase(LRUMap[key]);       //remove old place
        std::list<LRUNode>::iterator it = LRUlist.end();
        it--;
        LRUMap[key] = it;  // update Map
	
        return it->value.getResponseInfo();
      }
    }
  }
  
  //put value into the cache's list, then update it
  void put(const std::string & key, Response & value) {
    if (LRUMap.count(key) != 0)  //exist
    {
      std::unordered_map<std::string,std::string> tempInformation = (*LRUMap[key]).information;
      LRUlist.erase(LRUMap[key]);        // remove old place, add end
      LRUlist.emplace_back(key, value, tempInformation);  // insert to end
      std::list<LRUNode>::iterator it = LRUlist.end();
      it--;
      LRUMap[key] = it;  // update Map
      it->information["expireTime"] = to_string(value.getExpireTime());
      if (value.getRevalidate()) {
        it->information["revalidate"] = "true";
      }
      else {
        it->information["revalidate"] = "false";
      }
    }
    else {                               // not exist
      if (LRUlist.size() >= capacity) {  // not enough space
        LRUMap.erase(LRUlist.front().key);
        LRUlist.pop_front();
      }
      LRUlist.emplace_back(key, value);  // insert to end
      std::list<LRUNode>::iterator it = LRUlist.end();
      it--;
      LRUMap[key] = it;  // update Map
      it->information["expireTime"] = to_string(value.getExpireTime());
      if (value.getRevalidate()) {
        it->information["revalidate"] = "true";
      }
      else {
        it->information["revalidate"] = "false";
      }
    }
  }
};
