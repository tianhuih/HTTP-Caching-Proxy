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
#include <vector>

#include "ResponseParser.hpp"
/*This is the object for response, it would have a class ResponseParser, and it would store the type, expire info, revalidate info (all parsed by parser and returned) and response lines.*/
class Response {
private:
  ResponseParser myParser; //parser
  std::string responseInfo;
  std::string type;
  double arriveTime; //the time the response arrived 
  double expireTime; // the time it would expire
  bool revalidate;
  std::string ETAG;
  std::string lastModified;

public:
  Response(const std::string & resInfo, const std::string & rtype, const double rtime) :
    myParser(),
    responseInfo(resInfo),
    type(rtype),
    arriveTime(rtime) {
    expireTime = myParser.parseExpire(responseInfo) + arriveTime;
    revalidate = myParser.needValidate(responseInfo);
    try{
      lastModified = myParser.parseLastModified(responseInfo);
    }
    catch(cantFindMarkException & e){
      lastModified = "";
    }
    try{
      ETAG = myParser.parseEtag(responseInfo);
    }
    catch(cantFindMarkException & e){
      ETAG = "";
    }
  }

  //validation
  std::string getETAG() { return ETAG; }
  std::string getLastModified() { return lastModified; }
  bool getRevalidate() { return revalidate; }

  //response
  std::string getResponseInfo() { return responseInfo; }

  //for cache
  double getExpireTime() { return expireTime; }
  
  //check if the response can be cached, if can't return its reason
  std::string canCache() {
    if (type != "GET") {
      return "GET";
    }
    else if (responseInfo.find("HTTP/1.1 200 OK") == std::string::npos) {
      return "NOT 200 OK";
    }
    else if (myParser.canCache(responseInfo) != "Can cache") {
      return myParser.canCache(responseInfo);
    }
    else {
      return "Can cache";
    }
  }
};
