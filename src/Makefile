SOURCES_M=http-proxy.cpp
OBJS=$(patsubst %.cpp, %.o, $(SOURCES_M))
CPPFLAGS=-ggdb3 -Wall -Werror -pedantic -pthread -std=gnu++11

http-proxy: $(OBJS)
	g++ $(CPPFLAGS) -o http-proxy $(OBJS)
%.o: %.cpp selfDefinedExceptions.hpp proxy.hpp Request.hpp cache.hpp logger.hpp Response.hpp ResponseParser.hpp Timer.hpp
	g++ $(CPPFLAGS) -c $<
clean:
	rm http-proxy *~ *.o
