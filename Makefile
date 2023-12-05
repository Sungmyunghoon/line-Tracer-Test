CX = g++
CFLAGS = -g -Wall
CVFLAGS = `pkg-config opencv4 --cflags --libs`
DXLFLAGS = -I/usr/local/include/dynamixel_sdk -ldxl_x64_cpp

TARGET = dxl
OBJS = line_Tracer.o dxl.o

$(TARGET) : $(OBJS)
	$(CX) -o $(TARGET) $(OBJS) $(CFLAGS) $(DXLFLAGS) $(CVFLAGS)
line_Tracer.o : line_Tracer.cpp
	$(CX) -c line_Tracer.cpp $(CFLAGS) $(DXLFLAGS) $(CVFLAGS)
dxl.o : dxl.hpp dxl.cpp
	$(CX) -c dxl.cpp $(CFLAGS) $(DXLFLAGS) $(CVFLAGS)

.PHONY: all clean
all: $(TARGET)
clean:
	rm -rf $(TARGET) $(OBJS)