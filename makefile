.DEFAULT_GOAL := all
CXXFLAGS := -g -O0 -Wall -Werror
CXXFLAGS_CPP03 := $(CXXFLAGS) -std=c++03
CXXFLAGS_CPP11 := $(CXXFLAGS) -std=c++11

CPP_TARGETS := \
	file_io-2 \
	mutex-1 \
	mutex-2

MD_TARGETS := \
	notes.html

.PHONY: all
all: $(CPP_TARGETS) $(MD_TARGETS) terminate-c++03 terminate-c++11

.PHONY: clean
clean:
	rm -f $(CPP_TARGETS) $(MD_TARGETS) terminate-c++03 terminate-c++11

%: %.cpp
	g++ $(CXXFLAGS_CPP11) -o $@ $^ -lpthread

terminate-c++03: terminate.cpp
	g++ $(CXXFLAGS_CPP03) -o $@ $^ -lpthread
terminate-c++11: terminate.cpp
	g++ $(CXXFLAGS_CPP11) -o $@ $^ -lpthread

$(MD_TARGETS): %.html: %.md
	markdown $^ >$@

