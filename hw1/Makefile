.PHONY: logger

CC = g++-13

all: logger liblogger

logger:
	$(CC) -std=c++20 -Wall -g -o logger logger.cpp

liblogger:
	$(CC) -std=c++20 -Wall -shared -fPIC -o logger.so liblogger.cpp
