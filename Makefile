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


.PHONY: all clean clean_netns
all: router

clean:
	rm -f $(SRC_ROOT)/*.o router

clean_netns:
	sudo ip -all netns del

%.o: $(SRC_ROOT)/%.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

$(SRC_ROOT)/hal.o: HAL/src/linux/router_hal.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

router: $(OBJ) $(SRC_ROOT)/hal.o
	$(CXX) $^ -o $@ $(LDFLAGS) 

line:
	@echo "pc1 <---> r1 <---> r2 <---> r3 <---> pc2"
	@echo "pc1: 192.168.1.2/24"
	@echo "r1: 192.168.1.1/24, 192.168.3.1/24"
	@echo "r2: 192.168.3.2/24, 192.168.4.1/24"
	@echo "r3: 192.168.4.2/24, 192.168.5.2/24"
	@echo "pc2: 192.168.5.1/24"
	sudo ip netns add pc1
	sudo ip netns add r1
	sudo ip netns add r2
	sudo ip netns add r3
	sudo ip netns add pc2
	sudo ip link add pc1r1-pc1 type veth peer name pc1r1-r1
	sudo ip link add r1r2-r1 type veth peer name r1r2-r2
	sudo ip link add r2r3-r2 type veth peer name r2r3-r3
	sudo ip link add r3pc2-r3 type veth peer name r3pc2-pc2
	sudo ip link set pc1r1-pc1 netns pc1 
	sudo ip link set pc1r1-r1 netns r1
	sudo ip link set r1r2-r1 netns r1
	sudo ip link set r1r2-r2 netns r2
	sudo ip link set r2r3-r2 netns r2
	sudo ip link set r2r3-r3 netns r3
	sudo ip link set r3pc2-r3 netns r3
	sudo ip link set r3pc2-pc2 netns pc2
	sudo ip netns exec pc1 ip link set pc1r1-pc1 up
	sudo ip netns exec pc1 ip addr add 192.168.1.2/24 dev pc1r1-pc1
	sudo ip netns exec pc1 ip route add default via 192.168.1.1 dev pc1r1-pc1
	sudo ip netns exec pc1 ethtool -K pc1r1-pc1 tx off
	sudo ip netns exec r1 ip link set pc1r1-r1 up
	sudo ip netns exec r1 ip link set r1r2-r1 up
	sudo ip netns exec r1 ethtool -K pc1r1-r1 tx off
	sudo ip netns exec r1 ethtool -K r1r2-r1 tx off
	sudo ip netns exec r2 ip link set r1r2-r2 up
	sudo ip netns exec r2 ip link set r2r3-r2 up
	sudo ip netns exec r2 ethtool -K r1r2-r2 tx off
	sudo ip netns exec r2 ethtool -K r2r3-r2 tx off
	sudo ip netns exec r3 ip link set r2r3-r3 up
	sudo ip netns exec r3 ip link set r3pc2-r3 up
	sudo ip netns exec r3 ethtool -K r2r3-r3 tx off
	sudo ip netns exec r3 ethtool -K r3pc2-r3 tx off
	sudo ip netns exec pc2 ip link set r3pc2-pc2 up
	sudo ip netns exec pc2 ip addr add 192.168.5.1/24 dev r3pc2-pc2
	sudo ip netns exec pc2 ip route add default via 192.168.5.2 dev r3pc2-pc2
	sudo ip netns exec pc2 ethtool -K r3pc2-pc2 tx off
