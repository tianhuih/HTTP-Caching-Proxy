#include "selfDefinedExceptions.hpp"
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <vector>
/*This class is used for parsing the infomation of response lines.*/
class ResponseParser {
 private:
  double longestTime; // if there is no expire sign, fresh time will be set to this value, which is 2*INT_MAX
  std::map<std::string, int> month; // hashmap for month name and month number

 public:
  ResponseParser() {
    month["Jan"] = 0;
    month["Feb"] = 1;
    month["Mar"] = 2;
    month["Apr"] = 3;
    month["May"] = 4;
    month["Jun"] = 5;
    month["Jul"] = 6;
    month["Aug"] = 7;
    month["Sept"] = 8;
    month["Oct"] = 9;
    month["Nov"] = 10;
    month["Dec"] = 11;
    longestTime += INT_MAX;
    longestTime += INT_MAX;
  }

  // check if the response need revalidate
  bool needValidate(const std::string & responseInfo) {
    bool need = false;
    if (responseInfo.find("Cache-Control") != std::string::npos) {
      if (responseInfo.find("no-cache") != std::string::npos){
	need = true;
      }
      else if(getMarkedLine("max-age=", responseInfo) != ""){
	if(std::stoi(getMarkedLine("max-age=", responseInfo)) == 0) {
        need = true;
	}
      }
    }
    return need;
  }
  
  //check if the response can be cached, if not, return the reason
  std::string canCache(const std::string & responseInfo) {
    if (responseInfo.find("Cache-Control") != std::string::npos) {
      if (responseInfo.find("no-store") != std::string::npos) {
        return "no-store";
      }
      else if (responseInfo.find("private") != std::string::npos) {
        return "private";
      }
    }
    return "Can cache";
  }

  // parse the expire time for the response, if there is no expire sign, return the longest expire time
  double parseExpire(const std::string & responseInfo) {
    double freshtime = 0;
    //call different paring function based on different types of signs
    if (responseInfo.find("Cache-Control: ") != std::string::npos) {
      try{
        freshtime = freshExpireControl(responseInfo);
      }
      catch(cantFindMarkException & e){
        freshtime = longestTime;
      }
    }
    else if (responseInfo.find("Expires: ") != std::string::npos) {
      try{
        freshtime = freshExpireLastModifyExp(responseInfo, true);
      }
      catch(cantFindMarkException & e){
        freshtime = longestTime;
      }
    }
    else if (responseInfo.find("Last-Modified: ") != std::string::npos) {
      try{
        freshtime = freshExpireLastModifyExp(responseInfo, false);
      }
      catch(cantFindMarkException & e){
        freshtime = longestTime;
      }
    }
    else {
      freshtime = longestTime;
    }
    return freshtime;
  }
  
  //parse the last modified line, if not found, throw self defined exception
  std::string parseLastModified(const std::string & responseInfo){
    std::string lastModified = "";
    size_t pos = responseInfo.find("Last-Modified:");
    if (pos != std::string::npos) {
      while (responseInfo[pos] != ' ') {
        pos++;
      }
      pos++;
      while (responseInfo[pos] != '\r') {
        lastModified += responseInfo[pos];
        pos++;
      }
      return lastModified;
    }
    else{
      throw cantFindMarkException();
    }
  }

  //parse the etag line, if not found, throw self defined exception
  std::string parseEtag(const std::string & responseInfo){
    std::string ETAG = "";
     size_t pos1 = responseInfo.find("ETag:");
    if (pos1 != std::string::npos) {
      while (responseInfo[pos1] != ' ') {
        pos1++;
      }
      pos1++;
      while (responseInfo[pos1] != '\r') {
        ETAG += responseInfo[pos1];
        pos1++;
      }
      return ETAG;
    }
    else{
      throw cantFindMarkException();
    }
  }

 private:
  // parsing the control-cache situation for expire time, if no mag age, return longest value
  double freshExpireControl(std::string strToParse) {
    if (strToParse.find("max-age") != std::string::npos ||
        strToParse.find("s-maxage") != std::string::npos) {
      if (strToParse.find("max-age") != std::string::npos) {
        std::string maxAgeStr = getMarkedLine("max-age=", strToParse);
        return std::stoi(maxAgeStr);
      }
      else {
        std::string sMaxAgeStr = getMarkedLine("s-maxage=", strToParse); 
        return std::stoi(sMaxAgeStr);
      }
    }
    else {
      return longestTime;
    }
  }
// parsing the expires/last modified situation for expire time, if no mag age, return longest value
  double freshExpireLastModifyExp(std::string strToParse, bool isExp) {
    double freshTime;
    std::string date = getMarkedLine("Date: ", strToParse);
    std::string last = getMarkedLine("Last-Modified: ", strToParse);
    std::vector<std::vector<std::string> > dateTimes;
    std::vector<std::string> times;
    if (!date.empty() && !last.empty()) {
      dateTimes.push_back(getDateTime(date));
      dateTimes.push_back(getDateTime(last));
    }
    struct tm dateTM = toStructTM(dateTimes[0]);
    struct tm lastTM = toStructTM(dateTimes[1]);
    freshTime = difftime(mktime(&dateTM), mktime(&lastTM));
    if (!isExp) {
      freshTime /= 10;
    }
    return freshTime;
  }
  //parse the string for a line starting with mark, if not found, throw exception
  std::string getMarkedLine(std::string mark, std::string strToParse) {
    std::string markedStr;
    size_t pos = strToParse.find(mark);
    if (pos != std::string::npos) {
      strToParse = strToParse.substr(pos, mark.size());
      pos += mark.size();
      while (strToParse[pos] != '\r') {
        markedStr += strToParse[pos];
        pos++;
      }
      return markedStr;
    }
    else {
      throw cantFindMarkException();
    }
  }
  //convert time string to struct tm
  struct tm toStructTM(std::vector<std::string> times) {
    struct tm res;
    // if there is error in converting date number, print it and abort without moving foward with wrong struct tm
    try{
    res.tm_mday = std::stoi(times[0]);
    res.tm_mon = month[times[1]];
    res.tm_year = std::stoi(times[2]) - 1900;
    res.tm_hour = std::stoi(times[3]);
    res.tm_min = std::stoi(times[4]);
    res.tm_sec = std::stoi(times[5]);
    }
    catch(std::exception & e){
      std::cerr << e.what() << std::endl;
      throw;
    }
    return res;
  }

  //parse the date string to get the number of year, mon, day, h, m, s
  std::vector<std::string> getDateTime(std::string time) {
    std::vector<std::string> res;
    std::string day;
    std::string mon;
    std::string year;
    std::string hour;
    std::string min;
    std::string sec;

    size_t daySpc = time.find_first_of(" ");

    daySpc++;
    while (isdigit(time[daySpc])) {
      day += time[daySpc];
      daySpc++;
    }
    res.push_back(day);

    daySpc++;
    while (isalpha(time[daySpc])) {
      mon += time[daySpc];
      daySpc++;
    }
    res.push_back(mon);

    daySpc++;
    while (isdigit(time[daySpc])) {
      year += time[daySpc];
      daySpc++;
    }
    res.push_back(year);

    daySpc++;
    while (isdigit(time[daySpc])) {
      hour += time[daySpc];
      daySpc++;
    }
    res.push_back(hour);

    daySpc++;
    while (isdigit(time[daySpc])) {
      min += time[daySpc];
      daySpc++;
    }
    res.push_back(min);

    daySpc++;
    while (isdigit(time[daySpc])) {
      sec += time[daySpc];
      daySpc++;
    }
    res.push_back(sec);
    return res;
  }
};
