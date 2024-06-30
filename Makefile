
# If you're not building this package within Buildroot, then
# you need to uncomment & adapt the following three variables,
# and comment out the existing LDFLAGS variable (since it
# contains $(TARGET_DIR), which is not set outside the
# Buildroot environment.

#BR2 = ~/code/buildroot-2022.05.1
#CC = $(BR2)/output/host/bin/arm-buildroot-linux-gnueabihf-gcc
#LDFLAGS = -L $(BR2)/output/target/usr/lib

LDFLAGS = -L$(TARGET_DIR)/usr/lib
LDLIBS = -lVE -lvencoder -lMemAdapter

all: main

main: config.h output.h h264.h cam.h cam.o config.o h264.o cJSON.o output.o
cJSON.o: cJSON.h
h264.o: config.h output.h config.o output.o
cam.o: config.h config.o
output.o: config.h output.h config.o
config.o: config.h cJSON.h cJSON.o


clean:
	rm -f $(wildcard *.o)
	rm -f main

