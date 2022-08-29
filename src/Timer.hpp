#include <cstdio>
#include <cstdlib>
#include <string>
//this class deals with time related functions
class Timer{
private:
  long maxAge = 5000000000; // if expire time larger than this value( >100 years), consider as never expire
public:
  Timer() = default;
  // this function will return the current local time as a string
  std::string getCurrentDateTime(std::string s){
    time_t now = time(0);
    struct tm  tstruct;
    char  buf[80];
    tstruct = *localtime(&now);
    if(s=="now")
      strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    else if(s=="date")
      strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
    return std::string(buf);
  }

  //this function will return the local time as number
  double getCurrentSec(){
    return time(0);
  }

  //this function will convert a time number into the local time string
  std::string getlocalTimeStr(long timeNum){
    if(timeNum > maxAge){
      return "Never Expire";
    }
    else{
      time_t rawtime = timeNum;
      struct tm * timeinfo;
      timeinfo = localtime (&rawtime);
      std::string timeStr(asctime(timeinfo));
      return timeStr.substr(0,timeStr.size() -1);
    }
  }  
};
