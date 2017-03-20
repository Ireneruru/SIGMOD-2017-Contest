CXX := g++ -std=c++11
CXXFLAGS := -g -O3 -Iinclude -Llib
LIBRARIES := -lpthread -ltbb -ltbbmalloc

all: harness main

harness: harness.cpp
	$(CXX) $(CXXFLAGS) harness.cpp -o harness $(LIBRARIES)

main: main.cpp find.h trie.h token.h constant.h
	$(CXX) $(CXXFLAGS) main.cpp -o main $(LIBRARIES)

run: harness main
	./harness ./data/large_small.init ./data/large_small.work ./data/large_small.result ./main

small_run: harness main
	./harness ./data/small.init ./data/small.work ./data/small.result ./main

clean:
	rm -rf main submit harness

