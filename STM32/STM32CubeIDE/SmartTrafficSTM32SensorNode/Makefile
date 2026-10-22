CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -O2
CPPFLAGS = -I Core/Inc -I Drivers/STM32N6xx_HAL_Driver/Inc
AR = ar
TARGET = libstms_sensor_logic.a
SOURCES = Core/Src/sensor_node.cpp Core/Src/hal_sensor_node.cpp Core/Src/traffic_analytics.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS) $(TARGET)

.PHONY: all clean
