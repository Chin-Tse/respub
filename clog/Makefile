
CC=gcc 
INCDIR=./inc
CFLAGS = -g -I$(INCDIR) 
SRC=./src
TARGET_PATH=./lib

SOURCE  := $(wildcard $(SRC)/*.c)
OBJS    := $(patsubst %.c,%.o,$(SOURCE))
TARGET=ipv4_log

.PHONY : all

all :$(OBJS) 
	$(CC) $(CFLAGS) -g -o $(TARGET) $(OBJS) 

.PHONY : clean
clean:
	rm -f $(TARGET) $(OBJS)
