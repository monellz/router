CXX = g++
SRC_ROOT = src
BACKEND = LINUX
BUILD = build
LIB_INCLUDE = HAL/include
INCLUDE = include
CXXFLAGS = -O3 --std=c++11 -I $(INCLUDE) -I $(LIB_INCLUDE) -DROUTER_BACKEND_$(BACKEND) -Wno-psabi
LDFLAGS = -lpcap

COMPILATION = $(wildcard $(SRC_ROOT)/*.cpp)
OBJ = $(COMPILATION:.cpp=.o)


.PHONY: all clean
all: router

clean:
	rm -f $(SRC_ROOT)/*.o router

%.o: $(SRC_ROOT)/%.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(SRC_ROOT)/hal.o: HAL/src/linux/router_hal.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

router: $(OBJ) $(SRC_ROOT)/hal.o
	$(CXX) $^ -o $@ $(LDFLAGS) 
