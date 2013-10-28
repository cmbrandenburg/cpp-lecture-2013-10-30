.DEFAULT_GOAL := all
CXXFLAGS := -g -O0 -Wall -Werror -std=c++11

PROGS := \
	file_io-2 \
	mutex-1 \
	mutex-2

.PHONY: all
all: $(PROGS)

.PHONY: clean
clean:
	rm -f $(PROGS)

%: %.cpp
	g++ $(CXXFLAGS) -o $@ $^ -lpthread

