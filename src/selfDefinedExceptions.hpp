#include <exception>
//exception class for parser
class cantFindMarkException: std::exception{
public:
  virtual const char* what() const noexcept{
    return "can't find the parsing mark!";
  }
};

//exception class for proxy handle call
class proxyHandleException: std::exception{
private:
  const char* type;
public:
  proxyHandleException(const char* rtype):type(rtype){}
  virtual const char* what() const noexcept{
    return "proxy handle error: ";
  }
  const char* getType() const noexcept{
    return type;
  }
};
