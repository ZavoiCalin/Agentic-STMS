CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2
CPPFLAGS = -I Core/Inc
TARGET = stm32_sensor_node_demo
SOURCES = Core/Src/main.c Core/Src/sensor_node.c Core/Src/traffic_analytics.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
