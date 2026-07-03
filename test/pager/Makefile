CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O0 -g
test_alert_queue: test_alert_queue.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
	./$@
test_ai_usage: test_ai_usage.cpp
	$(CXX) $(CXXFLAGS) $(shell pkg-config --cflags libcjson) -o $@ $< $(shell pkg-config --libs libcjson)
	./$@
