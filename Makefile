CXX = g++
SRC_ROOT = src
BACKEND = LINUX
LIB_INCLUDE = HAL/include
INCLUDE = include
CXXFLAGS = -O3 --std=c++11 -I $(INCLUDE) -I $(LIB_INCLUDE) -DROUTER_BACKEND_$(BACKEND)
LDFLAGS = -lpcap

COMPILATION = $(wildcard $(SRC_ROOT)/*.cpp)
OBJ = $(COMPILATION:.cpp=.o)


.PHONY: all clean
all: router

clean:
	rm -f *.o router std

%.o: $(SRC_ROOT)/%.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

hal.o: HAL/src/linux/router_hal.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

router: $(OBJ) hal.o
	$(CXX) $^ -o $@ $(LDFLAGS) 
	rm $(OBJ) hal.o
