CXX = gcc
CXXFLAGS = -Wall -Werror -pedantic -Og -g -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG
OBJECTS = fs.o

all: disk_image

disk_image: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ *.c

clean:
	rm -f *.o disk_image

.PHONY: clean all
