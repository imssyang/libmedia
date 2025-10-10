ROOT_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
INSTALL_DIR := $(ROOT_DIR)/release
FFMPEG_HOME ?= /opt/ffmpeg

TARGET := libmedia
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
	CC = clang
	CXX = clang++
	CFLAGS = -Wall -Wextra -g -O0 -std=c++2a -I$(FFMPEG_HOME)/include
	LDFLAGS = -L$(FFMPEG_HOME)/lib -Wl,-rpath,$(FFMPEG_HOME)/lib \
		-lavcodec -lavformat -lavutil -lswscale -lswresample
	SHARED_LDFLAGS = -dynamiclib -install_name @rpath/$(TARGET).dylib
	STATIC_LIB = $(TARGET).a
	DYNAMIC_LIB = $(TARGET).dylib
else ifeq ($(UNAME), Linux)
	CC = gcc
	CXX = g++
	CFLAGS = -Wall -Wextra -fPIC -g -O0 -std=c++2a -I$(FFMPEG_HOME)/include
	LDFLAGS = -L$(FFMPEG_HOME)/lib -Wl,-rpath,'$ORIGIN:$(FFMPEG_HOME)/lib' \
		-lavcodec -lavformat -lavutil -lswscale -lswresample
	SHARED_LDFLAGS = -shared -fPIC
	STATIC_LIB = $(TARGET).a
	DYNAMIC_LIB = $(TARGET).so
else
	$(error Unsupported OS: $(UNAME))
endif

SRCS = ffmpeg.cpp \
	avmanage.cpp \
	avmedia.cpp \
	avformat.cpp \
	avcodec.cpp \
	avutil.cpp \
	swscale.cpp \
	swresample.cpp
OBJS = $(SRCS:.cpp=.o)

TEST_TARGET = tests/main
TEST_SRCS = tests/main.cpp \
	tests/test_ffmpeg.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)

all: $(STATIC_LIB) $(DYNAMIC_LIB) $(TEST_TARGET)
	@echo "ROOT_DIR: $(ROOT_DIR)"
	@echo "INSTALL_DIR: $(INSTALL_DIR)"

$(STATIC_LIB): $(OBJS)
	ar rcs $@ $^

$(DYNAMIC_LIB): $(OBJS)
	$(CXX) -o $@ $^ $(SHARED_LDFLAGS) $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJS) $(STATIC_LIB)
	$(CXX) -o $@ -I. -L. $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

test:
	rm -rf nohup.out
	nohup ./$(TEST_TARGET)

install:
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(INSTALL_DIR)/lib
	cp *.h $(INSTALL_DIR)/include
	cp $(STATIC_LIB) $(INSTALL_DIR)/lib
	cp $(DYNAMIC_LIB) $(INSTALL_DIR)/lib

clean:
	rm -rf $(OBJS) $(TEST_OBJS) \
		$(STATIC_LIB) $(DYNAMIC_LIB) \
		$(TARGET) $(TARGET).dSYM $(TEST_TARGET) \
		$(INSTALL_DIR) nohup.out

.PHONY: all clean run
