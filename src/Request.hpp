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

/*This class is for reuqest strings, designed for high cohesion, it will get the recivevd clinet request string, parse the hostname and port itself and save the string, the host name and the port to fields, the use get to get them. If there is no port, port will be an empty string. During the call of Proxy::getServersendbrowser, port need to be checked. */

class Request {
private:
  std::string requestInfo;
  std::string key;
  std::string hostname;
  std::string port;
  std::string type;
  
  //parse the request lines for hostname and port
  void parseRequest() {
    key = requestInfo.substr(0, requestInfo.find_first_of("\r\n"));
    size_t start = 0;
    while (isupper(requestInfo[start])) {
      type += requestInfo[start];
      start++;
    }
    size_t pos = requestInfo.find("Host: ");  //get hostname
    std::string hostLine = requestInfo.substr(pos + strlen("Host: "));
    size_t endPos = hostLine.find_first_of("\r\n");
    hostLine = hostLine.substr(0, endPos); 
    size_t portPos = hostLine.find(":"); // get port
    if (portPos != std::string::npos) {
      hostname = hostLine.substr(0, portPos);
      port = hostLine.substr(portPos + 1);
    }
    else {
      port = ""; // if there is no port, just store empty str
      hostname = hostLine;
    }
  }

public:
  //This is for validation, it would add the revalidation lines into the request string and for sending back
  void setRequest(std::string tobeAdd) {
    size_t pos = requestInfo.find("\r\n\r\n");
    requestInfo += tobeAdd;
    pos +=2;
    for (size_t i = 0; i < tobeAdd.size(); i++) {
      requestInfo[pos] = tobeAdd[i];
      pos++;
    }
    requestInfo[pos] = '\r';
    pos++;
    requestInfo[pos] = '\n';
  }
  Request(const std::string & reqInfo) : requestInfo(reqInfo) {
    parseRequest(); //call the parsing method once constructed
  }

  std::string getHostname() { return hostname; }

  std::string getPort() { return port; }

  std::string getType() { return type; }

  std::string getKey() { return key; }

  std::string getRequestInfo() { return requestInfo; }
};
