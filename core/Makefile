CXX = g++
CXXFLAGS = -O2 -Wall -std=c++11

all: tokenizer indexer search_cli

tokenizer: tokenizer.cpp mystr.h
	$(CXX) $(CXXFLAGS) -o tokenizer tokenizer.cpp

indexer: indexer.cpp mystr.h vec.h map.h porter.h
	$(CXX) $(CXXFLAGS) -o indexer indexer.cpp

search_cli: search_cli.cpp mystr.h vec.h map.h porter.h
	$(CXX) $(CXXFLAGS) -o search_cli search_cli.cpp

clean:
	rm -f tokenizer indexer search_cli

.PHONY: all clean
