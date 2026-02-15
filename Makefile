CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

SRCS = src/main.cpp src/aiger_parser.cpp src/cnf_generator.cpp src/proof_parser.cpp src/interpolant.cpp src/model_checker.cpp
OBJS = $(SRCS:.cpp=.o)

bmc: $(OBJS)
	$(CXX) $(CXXFLAGS) -o bmc $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f bmc src/*.o out.cnf proof.txt result.txt

test: bmc
	./bmc 10 test.aag