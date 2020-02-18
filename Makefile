SOURCES_M=ringmaster.cpp
OBJS_M=$(patsubst %.cpp, %.o, $(SOURCES_M))
SOURCES_P=player.cpp
OBJS_P=$(patsubst %.cpp, %.o, $(SOURCES_P))
CPPFLAGS=-ggdb3 -Wall -Werror -pedantic -std=gnu++11

all: ringmaster player

ringmaster: $(OBJS_M)
	g++ $(CPPFLAGS) -o ringmaster $(OBJS_M)
player: $(OBJS_P)
	g++ $(CPPFLAGS) -o player $(OBJS_P)
%.o: %.cpp potato.hpp
	g++ $(CPPFLAGS) -c $<

clean:
	rm test *~ *.o
