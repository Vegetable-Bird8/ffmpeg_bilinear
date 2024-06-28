# gcc -o test_resize main.cpp opt.c ... -Iinclude -lstdc++
CC = gcc
CXX = g++
CFLAGS = -Ilibavutil -D__STDC_CONSTANT_MACROS -fpermissive -std=c99 -g -fPIE -no-pie
LDFLAGS = -lstdc++
# 忽略的文件夹
IGNORED_DIRS = trash

# 获取除了忽略文件夹外的所有源文件
# SOURCES := $(filter-out $(wildcard $(addsuffix /*.c, $(IGNORED_DIRS))), $(SOURCES))
SRCS = main.c hscale.c initFilter.c mem.c output.c pixdesc.c slice.c vscale.c input.c# swscale.c #dict.c  opt.c options.c rational.c
OBJS = $(SRCS:.c=.o)

test_resize: $(OBJS)
	$(CXX) -o $@ $^ $(CFLAGS) $(LDFLAGS) -fPIE -no-pie

# %.o: %.cpp
# 	$(CXX) -c -o $@ $< $(CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(OBJS) test_resize *.yuv
