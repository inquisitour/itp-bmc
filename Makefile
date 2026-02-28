CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -O2 -Wall
CFLAGS = -O2 -Wall

SRCS = src/main.cpp src/aiger_parser.cpp src/cnf_generator.cpp src/proof_parser.cpp src/interpolant.cpp src/model_checker.cpp
OBJS = $(SRCS:.cpp=.o) aiger-1.9.9/aiger.o

bmc: $(OBJS)
	$(CXX) $(CXXFLAGS) -o bmc $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

aiger-1.9.9/aiger.o: aiger-1.9.9/aiger.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f bmc src/*.o aiger-1.9.9/aiger.o out.cnf proof.txt result.txt

test: bmc
	BMC_WORKDIR=. ./bmc 5 demo/buggy.aag; \
	BMC_WORKDIR=. ./bmc 20 demo/safe_small.aag; \
	BMC_WORKDIR=. ./bmc 10 demo/safe_medium.aag; \
	BMC_WORKDIR=. ./bmc 20 demo/s298.aag; \
	BMC_WORKDIR=. ./bmc 20 demo/bakery.aag