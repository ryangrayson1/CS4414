CXX = gcc
CXXFLAGS = -Wall -Werror -pedantic -Og -g -fsanitize=address -fsanitize=undefined -D_GLIBCXX_DEBUG
OBJECTS = fs.o

all: disk_image

disk_image: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ *.c

SUBMIT_FILENAME=dragon-fs-submission-$(shell date +%Y%m%d%H%M%S).tar.gz

submit:
	tar -zcf $(SUBMIT_FILENAME) inode.h fs.c Makefile README

clean:
	rm -f *.o disk_image

.PHONY: clean all
