CXX = g++
CXXFLAGS = -Wall -Werror -pedantic -Og -g -std=c++17 -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG
OBJECTS = fs.o

all: disk_image

disk_image: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ *.cc

clean:
	rm -f *.o disk_image

.PHONY: clean all
