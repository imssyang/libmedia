ROOT_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
INSTALL_DIR := $(ROOT_DIR)/release
FFMPEG_HOME ?= $(ROOT_DIR)/deps/ffmpeg-7.1

TARGET := libmedia
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
	CXX = clang++
	CXXFLAGS = -Wall -Wextra -g -O0 -std=c++20
	CPPFLAGS = -I$(FFMPEG_HOME)/include
	LDFLAGS = \
		-L$(FFMPEG_HOME)/lib \
		-Wl,-rpath,$(FFMPEG_HOME)/lib \
		-lavcodec -lavformat -lavutil -lswscale -lswresample
	SHARED_LDFLAGS = -dynamiclib -install_name @rpath/$(TARGET).dylib
	STATIC_LIB = $(TARGET).a
	DYNAMIC_LIB = $(TARGET).dylib
else ifeq ($(UNAME), Linux)
	CXX = g++
	CXXFLAGS = -Wall -Wextra -fPIC -g -O0 -std=c++20
	CPPFLAGS = -I$(FFMPEG_HOME)/include
	LDFLAGS = \
		-L$(FFMPEG_HOME)/lib \
		-Wl,-rpath,'$$ORIGIN' \
		-Wl,-rpath,$(FFMPEG_HOME)/lib \
		-lavcodec -lavformat -lavutil -lswscale -lswresample
	SHARED_LDFLAGS = -shared -fPIC
	STATIC_LIB = $(TARGET).a
	DYNAMIC_LIB = $(TARGET).so
else
	$(error Unsupported OS: $(UNAME))
endif

SRCS = \
	src/ffmpeg.cpp \
	src/avmanage.cpp \
	src/avmedia.cpp \
	src/avformat.cpp \
	src/avcodec.cpp \
	src/avutil.cpp \
	src/swscale.cpp \
	src/swresample.cpp
OBJS = $(SRCS:.cpp=.o)

TEST_TARGET := tests/main
TEST_SRCS := \
	tests/main.cpp \
	tests/test_ffmpeg.cpp
TEST_OBJS = $(TEST_SRCS:.cpp=.o)

.PHONY: all clean test install

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
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

install:
	mkdir -p $(INSTALL_DIR)/include/$(TARGET)
	mkdir -p $(INSTALL_DIR)/lib
	cp src/*.h $(INSTALL_DIR)/include/$(TARGET)
	cp $(STATIC_LIB) $(INSTALL_DIR)/lib
	cp $(DYNAMIC_LIB) $(INSTALL_DIR)/lib

clean:
	rm -rf $(OBJS) $(TEST_OBJS) \
		$(STATIC_LIB) $(DYNAMIC_LIB) \
		$(TARGET) $(TARGET).dSYM $(TEST_TARGET) \
		$(INSTALL_DIR) nohup.out
