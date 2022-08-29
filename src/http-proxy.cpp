#include "proxy.hpp"

/*The entry function of threads*/
int threadCommunicate(int sockfd_to_browser, Proxy& myProxy, Cache & myCache, Timer & myTimer, logger & myLogger, int ID ){
  //each thread has its own ID, sockfd to accept, but share one looger, timer and cache and proxy
  std::string requestInfo;
  if(myProxy.getRequest(requestInfo,sockfd_to_browser)){
    close(sockfd_to_browser);
    return EXIT_FAILURE;
  }
  Request reqObj(requestInfo);

  std::string hostConnect = reqObj.getHostname();
  std::string portConnect = reqObj.getPort();
  if(portConnect.empty()){
    portConnect = "80"; // if port not specified, use default value 80
  }

  int sockfd_to_origin = myProxy.connectToSocket(hostConnect.c_str(),portConnect.c_str());
  if(sockfd_to_origin < 0){
    std::cerr << "connect to origin host error" << std::endl;
    close(sockfd_to_browser);
    close(sockfd_to_origin);
    return EXIT_FAILURE;
  }
  // get the type of request and call corresponding function
  if (reqObj.getType() == "GET") {
    try{myProxy.handleGet(reqObj, myCache, myTimer, myLogger, ID, sockfd_to_browser, sockfd_to_origin);
    }
    catch(proxyHandleException& e){ // if there is an error in handling, print error message, close sockfds, return 
      close(sockfd_to_browser);
      close(sockfd_to_origin);
      std::cerr << e.what() << e.getType() << std::endl;
      return EXIT_FAILURE;
    }
  }
  else if (reqObj.getType() == "POST") {
    try{
      myProxy.handlePost(reqObj, myTimer, myLogger, ID, sockfd_to_browser, sockfd_to_origin);
    }
    // if there is an error in handling, print error message, close sockfds, return 
    catch(proxyHandleException& e){
      close(sockfd_to_browser);
      close(sockfd_to_origin);
      std::cerr << e.what() << e.getType() << std::endl;
      return EXIT_FAILURE;
    }
  }
  else {
    try{
      myProxy.handleConnect(reqObj, myLogger, ID, sockfd_to_browser, sockfd_to_origin);
    }
    // if there is an error in handling, print error message, close sockfds, return 
    catch(proxyHandleException& e){
      close(sockfd_to_browser);
      close(sockfd_to_origin);
      std::cerr << e.what() << e.getType() << std::endl;
      return EXIT_FAILURE;
    }
  }
  close(sockfd_to_browser);
  close(sockfd_to_origin);
  return EXIT_SUCCESS;
}

int main(int argc, char * argv[]) {
  if (argc < 2) {
    std::cerr << "wrong number of arguments" << std::endl;
  }
  const char * hostname = "0.0.0.0"; //listen on any host
  const char * port = argv[1]; // input port 12345
  Proxy myProxy(hostname, port);
  Timer myTimer;
  logger myLogger;
  Cache myCache(200); //200 is the size of cache
  int ID = 0;
  int sockfd_own = 0;
  //setup a socket and listen it at first
  //when the main thread calls system call function of proxy class directly, it must handle the system call error
  try{
    sockfd_own = myProxy.setupSocket(hostname,port);
  }
  catch(std::exception & e){
    std::cerr << "setup socket error!" << std::endl;
    return EXIT_FAILURE;
  }

  //repeat the accept, if there is any incoming request, give the sockfd to a thread and let it handle it
  while (true) {
    std::string requestInfo;
    int sockfd_to_browser = 0;
    try{
      sockfd_to_browser = myProxy.acceptSocket(sockfd_own);
    }
    catch(std::exception & e){
      std::cerr << "setup socket error!" << std::endl;
      close(sockfd_own);
      return EXIT_FAILURE;
    }
    ID++;
    try{
      std::thread(threadCommunicate, sockfd_to_browser, std::ref(myProxy), std::ref(myCache), std::ref(myTimer), std::ref(myLogger), ID).detach();
    }
    catch(std::exception& e){
      close(sockfd_own);
      return EXIT_SUCCESS;
    }
  }
  close(sockfd_own);
  return EXIT_SUCCESS;
}
