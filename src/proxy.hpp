#include "cache.hpp"
#include "logger.hpp"
#include <thread>
/*This class is the top class for the proxy*/
std::mutex cacheMutex;
class Proxy {
private:
  const char * hostname;
  const char * port;
  //public helper functions that rely on it's own private methods. called in main (http-proxy.cpp) 
public:
  Proxy(const char * rhostname, const char * rport) : hostname(rhostname), port(rport) {}
  //call accept system call and accept a socket from browser, return the sockfd.
  // would be called by main thread, throw exception(to get main thread return) once error
  int acceptSocket(int sockfd_own){
    struct sockaddr_storage connector_addr;
    socklen_t sin_size;
    sin_size = sizeof connector_addr;
    int sockfd_to_browser = accept(sockfd_own, (struct sockaddr *)&connector_addr, &sin_size);
    if (sockfd_to_browser == -1) {
      std::perror("accept");
      throw std::exception();
    }
    return sockfd_to_browser;
  }
  
  //setup the proxy's own socket and return the sockfd, if any error, print and return
  // would be called by main thread, close the openned sockfd and throw exception(to get main thread return) once error
  int setupSocket(const char* hostname, const char* port){
    int status;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;

    memset(&host_info, 0, sizeof(host_info));

    host_info.ai_family   = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
    host_info.ai_flags    = AI_PASSIVE;

    status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  
    if (status != 0) {
      std::cerr << "Error: cannot get address info for host" << std::endl;
      std::cerr << "  (" << hostname << "," << port << ")" << std::endl;
      throw std::exception();
    }
  
    int sockfd_own = socket(host_info_list->ai_family, 
                            host_info_list->ai_socktype, 
                            host_info_list->ai_protocol);
    if (sockfd_own == -1) {
      std::cerr << "Error: cannot create socket" << std::endl;
      std::cerr << "  (" << hostname << "," << port << ")" << std::endl;
      throw std::exception();
    }

    int yes = 1;
    status = setsockopt(sockfd_own, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    status = bind(sockfd_own, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
      std::cerr << "Error: cannot bind socket" << std::endl;
      std::cerr << "  (" << hostname << "," << port << ")" << std::endl;
      close(sockfd_own);
      throw std::exception();
    }

    status = listen(sockfd_own, 100);
    if (status == -1) {
      std::cerr << "Error: cannot listen on socket" << std::endl; 
      std::cerr << "  (" << hostname << "," << port << ")" << std::endl;
      close(sockfd_own);
      throw std::exception();
    }
    freeaddrinfo(host_info_list);
    return sockfd_own;
  }
  
  //take the hostname and port, connect to the demanded server
  //would be called by other threads, return -1 on error, it would trigger a exception throwing in the handleXXX method, the cause the thread return
  int connectToSocket(const char* hostname, const char* port){
    struct addrinfo host_info;
    struct addrinfo *host_info_list;

    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family   = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
    host_info.ai_flags = AI_PASSIVE;
  
    int status = getaddrinfo(hostname, port, &host_info, &host_info_list);
    if (status != 0) {
      std::cerr << gai_strerror(status) << std::endl;
      std::cerr << "Error: cannot get address info for host" << std::endl;
      std::cerr << "(" << hostname << "," << port << ")" << std::endl;
      return -1;
    } 
  
    int sockfd_to_origin = socket(host_info_list->ai_family, 
                                  host_info_list->ai_socktype, 
                                  host_info_list->ai_protocol);
    if (sockfd_to_origin == -1) {
      std::perror("build socket");
      std::cerr << "Error: cannot create socket" << std::endl;
      std::cerr << "  (" << hostname << ":" << port << ")" << std::endl;
      return -1;
    }
  
    status = connect(sockfd_to_origin, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
      std::perror("connect client");
      std::cerr << "Error: cannot connect to socket" << std::endl;
      std::cerr << "  (" << hostname << ":" << port << ")" << std::endl;
      return -1;
    }
    freeaddrinfo(host_info_list);
    return sockfd_to_origin;
  }
  
  //get the request info from the browser
  int getRequest(std::string& request, int SockfdB){
    if(!recieve(SockfdB,request)){
      std::cerr << "accept from browser" << std::endl;
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  //the method that handles any GET request, cache is involved, throws exception on error
  int handleGet(Request reqObj,
                Cache & myCache,
                Timer & myTimer,
                logger & myLogger,
                int ID, int sockfdB, int sockfdO) {
    //print a request Info to log
    myLogger.getrequest_time(ID, reqObj);
    myLogger.print_recieve_requestline();

    cacheMutex.lock(); //try to get from the cache, protected by mutex
    std::string cached = myCache.get(reqObj.getKey(), myTimer.getCurrentSec());
    cacheMutex.unlock(); 

    //act differently (revalidate, reuse, get again according to the result of cache get)
    if (cached != "notfound" && cached.find("revalidate:") == std::string::npos &&
        cached[0] != 'I' && cached.find("expires:") == std::string::npos) {
      if (sendCacheBrowser(cached, sockfdB)) {
        std::cerr << "send cached error!" << std::endl;
        throw proxyHandleException("GET");
      }
      //in cache, reuse, print in cache, respond to log
      myLogger.printCache("in cache, valid", ID);
      myLogger.getresponse_send(ID, cached);
      myLogger.print_response_sendline();

    }
    else if (cached == "notfound" || cached.find("expires:") != std::string::npos) {
      std::string getInfo;
      if (getServerSendBrowser(reqObj.getRequestInfo(), getInfo, sockfdB, sockfdO)) {

        throw proxyHandleException("GET");
      }
      Response resObj(getInfo, reqObj.getType(), myTimer.getCurrentSec());

      if (cached == "notfound") {
        myLogger.printCache("not in cache", ID);
      }
      else {
        std::string expireString = "in cache, but expired at ";
        cached = cached.substr(cached.find_first_of(':') + 1);
        long expireInt = std::stol(cached);
        expireString += myTimer.getlocalTimeStr(expireInt);
        myLogger.printCache(expireString, ID);
      }

      myLogger.getrequest_requesting(ID, reqObj);
      myLogger.print_send_requestline();
      myLogger.getresponse_recieve(ID, resObj.getResponseInfo(), reqObj.getHostname());
      myLogger.print_response_recieveline();

      if (resObj.canCache() == "Can cache") {
        cacheMutex.lock(); /////////
        myCache.put(reqObj.getKey(), resObj);
        cacheMutex.unlock(); /////////

        if (resObj.getRevalidate()) {
          myLogger.printCache("cached, but requires re-validation", ID);
        }
        else {
          myLogger.printCache("cached, expires at " + myTimer.getlocalTimeStr(resObj.getExpireTime()), ID);
        }
      }
      else {
        myLogger.printCache("not cacheable because " + resObj.canCache(), ID);
      }

      myLogger.getresponse_send(ID, resObj.getResponseInfo());
      myLogger.print_response_sendline();

    }
    //Need revalidate
    else {
      cached = cached.substr(cached.find_first_of(':') + 1);
      reqObj.setRequest(cached);
      std::string getInfo;
      if (getServerSendBrowser(reqObj.getRequestInfo(), getInfo, sockfdB, sockfdO)) {
        throw proxyHandleException("GET");
      }

      myLogger.printCache("in cache, requires validation", ID);

      if (getInfo.find("304 Not Modified") != std::string::npos) {
        //Get from cache directly
        cacheMutex.lock(); /////////
        string temp = myCache.noModifyGet(reqObj.getKey());
        cacheMutex.unlock(); /////////
        if (sendCacheBrowser(temp, sockfdB)) {
          std::cerr << "get error!" << std::endl;
          throw proxyHandleException("GET");
        }
        myLogger.printCache("NOTE 304 Not Modified, revalidate success", ID);
        myLogger.getresponse_send(ID, temp);
        myLogger.print_response_sendline();
      }
      else {
        std::string getInfo;
        if (getServerSendBrowser(reqObj.getRequestInfo(), getInfo, sockfdB, sockfdO)) {
          throw proxyHandleException("GET");
        }
        Response resObj(getInfo, reqObj.getType(), myTimer.getCurrentSec());
        myLogger.printCache("NOTE revalidate fail", ID);
        myLogger.getrequest_requesting(ID, reqObj);
        myLogger.print_send_requestline();
        myLogger.getresponse_recieve(ID, resObj.getResponseInfo(), reqObj.getHostname());
        myLogger.print_response_recieveline();
	if (resObj.canCache() == "Can cache") {
          cacheMutex.lock();/////////////////
          myCache.put(reqObj.getKey(), resObj);
          cacheMutex.unlock();//////////////
          if (resObj.getRevalidate()) {
            myLogger.printCache("cached, but requires re-validation", ID);
          }
          else {
            myLogger.printCache("cached, expires at " + myTimer.getlocalTimeStr(resObj.getExpireTime()),ID);
          }
        }
        else {
          myLogger.printCache("not cacheable because " + resObj.canCache(), ID);
        }
        myLogger.getresponse_send(ID, resObj.getResponseInfo());
        myLogger.print_response_sendline();
      }
    }
    return EXIT_SUCCESS;
  }

  // this method is for handling POST requests, called by threads, throws exception on error
  int handlePost(Request reqObj, Timer & myTimer, logger & myLogger, int ID, int sockfdB, int sockfdO) {
    //print request line in log
    myLogger.getrequest_time(ID, reqObj);
    myLogger.print_recieve_requestline();
    std::string getInfo;
    //recv info from host then send back to browser
    if (getServerSendBrowser(reqObj.getRequestInfo(), getInfo, sockfdB, sockfdO)) {
      throw proxyHandleException("POST");
    }
    Response resObj(getInfo, reqObj.getType(), myTimer.getCurrentSec());
    myLogger.getrequest_requesting(ID, reqObj);
    myLogger.print_send_requestline();
    myLogger.getresponse_recieve(ID, resObj.getResponseInfo(), reqObj.getHostname());
    myLogger.print_response_recieveline();
    myLogger.getresponse_send(ID, resObj.getResponseInfo());
    myLogger.print_response_sendline();
    return EXIT_SUCCESS;
  }

  // this method is for handling CONNECT requests, called by threads, throws exception on error
  int handleConnect(Request reqObj, logger & myLogger, int ID, int sockfdB, int sockfdO) {
    myLogger.getrequest_time(ID, reqObj);
    myLogger.print_recieve_requestline();
    myLogger.getrequest_requesting(ID, reqObj);
    myLogger.print_send_requestline();
    //using select, listen on both sides, send info
    if (selectBrowserServer(sockfdB, sockfdO)) {
      throw proxyHandleException("CONNECT");
    }
    myLogger.printCache("Tunnel closed", ID);
    return EXIT_SUCCESS;
  }

private:
  //using select, listen on both sides, send info
  int selectBrowserServer(int SockfdB, int SockfdO){    
    if(!Send(SockfdB,"HTTP/1.1 200 OK\r\n\r\n")){
      std::cerr << "send 200 OK to client error" << std::endl;
      return EXIT_FAILURE;
    }

    int fdmax = ((SockfdB > SockfdO) ? SockfdB : SockfdO);
    fd_set sockfds;
    FD_ZERO(&sockfds);
    fd_set sockfds_temp;
    FD_ZERO(&sockfds_temp);
    FD_SET(SockfdB, &sockfds);
    FD_SET(SockfdO, &sockfds);
    while(true){
      sockfds_temp = sockfds; // copy it
      if (select(fdmax+1, &sockfds_temp, NULL, NULL, NULL) == -1) {
        std::perror("select");
        return EXIT_FAILURE;
      }
      for(int i = 0; i <= fdmax; i++) {
        if (FD_ISSET(i, &sockfds_temp)) {
          if(i == SockfdB){
            char temp[65536];
            int numbytes = 0;
            if((numbytes = recv(SockfdB, temp, 65536, 0)) == -1) {
              std::perror("recv client");
              return EXIT_FAILURE;
            }
            //    std::cout << numbytes << std::endl;
            if(numbytes == 0){
              //std::cout << "BOver!" << std::endl;
              return EXIT_SUCCESS;
            }

            if (send(SockfdO, temp, numbytes, 0) == -1){ 
              std::perror("connect send to host");
              return EXIT_FAILURE;
            }
            continue;
          }
          else if(i == SockfdO){
            char temp[65536];
            int numbytes = 0;
            if((numbytes = recv(SockfdO, temp, 65536, 0)) == -1) {
              std::perror("recv server");
              return EXIT_FAILURE;
            }
            //std::cout << numbytes << std::endl;
            if(numbytes == 0){
              //std::cout << "OOver!" << std::endl;
              return EXIT_SUCCESS;
            }
            
            if (send(SockfdB, temp, numbytes, 0) == -1){ 
              std::perror("connect send to client");
              return EXIT_FAILURE;
            }
            continue;
          }
          else{
            continue;
            std::cout << "Wrong fd!" << std::endl;
          }
        }
      }
    }
    return EXIT_SUCCESS;
  }
  
  //recv info from host then send back to browser
  int getServerSendBrowser(const std::string& requestInfo, std::string& getInfo, int SockfdB, int SockfdO){
    if(!Send(SockfdO,requestInfo)){
      std::cerr << "send to origin error" << std::endl;
      return EXIT_FAILURE;
    }
      
    int chunck = recieve_origin(SockfdO,getInfo);
    if(chunck < 0){
      std::cerr << "recv from origin error" << std::endl;
      return EXIT_FAILURE;
    }
    if(chunck > 0){
      if(!recieve_unchunck(SockfdO,getInfo,chunck)){
        std::cerr << "recv from unchuncked origin error" << std::endl;
        return EXIT_FAILURE;
      }
      //std::cout <<"******Send: "<<getInfo << std::endl;
      if(!Send(SockfdB, getInfo)){
        std::cerr << "send to browser" << std::endl;
        return EXIT_FAILURE;
      }
    }
    else{
      if(!recieve_chunck(SockfdB, SockfdO, getInfo)){
        std::cerr << "recv from chuncked origin error" << std::endl;
        return EXIT_FAILURE;
      }
    }
    return EXIT_SUCCESS;
  }
  
  int sendCacheBrowser(const std::string& cachedInfo, int SockfdB){
    if(!Send(SockfdB,cachedInfo.c_str())){
      std::cerr << "send to origin error" << std::endl;
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  
  bool Send(int sendFd, std::string toSend){
    if (send(sendFd, toSend.c_str(), toSend.size(), 0) == -1){ 
      std::perror("send port number");
      return false;
    }
    return true;
  }

  int getLength(std::string& recv){
    size_t pos = recv.find("Content-Length: ");
    if(pos == std::string::npos){
      std::cerr << "can't find length!" << std::endl;
      return -1;
    }
    else{
      std::string lengthStr;
      while(recv[pos]!='\n'){
        if(isdigit(recv[pos])){
          lengthStr += recv[pos];
        }
        pos ++;
      }
      return std::stoi(lengthStr);
    }
  }

  int getHeadLength(std::string& recv){
    size_t pos = recv.find("\r\n\r\n");
    if(pos == std::string::npos){
      std::cerr << "can't find empty line!" << std::endl;
      return -1;
    }
    return pos;
  }

  bool recieve_unchunck(int sockfd, std::string& toGet, int hasGot){
    int allLen = 0;
    int numbytes = 0;
    if(toGet.find("HTTP/1.1 200 OK") == std::string::npos){
      allLen = 0;
    }
    else{
      int len = getLength(toGet);
      if(len < 0){
        return false;
      }
      int headlen = getHeadLength(toGet);
      if(headlen < 0){
        return false;
      }
      allLen = headlen + len;
    }
    int i = 0;
    while(hasGot < allLen){
      std::vector<char> temp;
      temp.resize(65536);
      if((numbytes = recv(sockfd, &temp[0], 65535, 0)) == -1) {
        std::perror("recv");
        return false;
      }
      hasGot += numbytes;
      temp.resize(numbytes);
      std::string tempStr(temp.begin(),temp.end());
      if(hasGot >= allLen){
        toGet += tempStr;	
        break;
      }
      else{
        toGet += tempStr;
      }
      i++;
    }
    return true;
  }
    
  bool recieve_chunck(int SockfdB, int SockfdO, std::string& toGet){
    if(!Send(SockfdB,toGet)){
      std::cerr << "send to browser" << std::endl;
      return false;
    }
    std::string endMark = "0\r\n\r\n";
    while(true){
      char temp[65536];
      int numbytes = 0;
      if((numbytes = recv(SockfdO, temp, 65536, 0)) == -1) {
        std::perror("chuncked recv server");
        return false;
      }
      if (send(SockfdB, temp, numbytes, 0) == -1){ 
        std::perror("chuncked send to client");
        return false;
      }
      std::string tempStr(temp);
      if(tempStr.find(endMark)!=std::string::npos || numbytes == 0){
        return true;
      }
    }
    return true;
  }
    
  bool isChuncked(const std::string& header){
    bool chunck = false;
    if(header.find("Transfer-Encoding:") != std::string::npos){
      if(header.find("chunked") != std::string::npos){
        chunck = true;
      }
    }
    return chunck;
  }
  
  int recieve_origin(int sockfd, std::string& toGet){
    int numbytes = 0;
    std::vector<char> temp;
    temp.resize(65536);
    if((numbytes = recv(sockfd, &temp[0], 65535, 0)) == -1) {
      std::perror("recv");
      return -1;
    }
    temp.resize(numbytes);
    std::string tempStr(temp.begin(),temp.end());
    toGet += tempStr;
    if(tempStr.find("HTTP/1.1 200 OK") == std::string::npos){
      return numbytes;
    }
    else{
      if(isChuncked(tempStr)){
        return 0;
      }
    }
    return numbytes;
  }
  
  bool recieve(int sockfd, std::string& toGet){
    int numbytes = 0;
    char temp[65536];
    while(true){
      memset(temp,'\0',sizeof(temp));
      std::string endMark = "\r\n\r\n";
      if((numbytes = recv(sockfd, temp, 65535, 0)) == -1) {
        std::perror("recv");
        return false;
      }
      std::string tempStr(temp);
      if(tempStr.find(endMark)!=std::string::npos){
        toGet += tempStr;
        break;
      }
      else{
        toGet += tempStr;
      }
    }
    return true;
  }

};
