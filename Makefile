CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra

all: bin/experiments bin/tests
bin/experiments: src/experiments.cpp src/cms.hpp src/stream.hpp
	@mkdir -p bin results
	$(CXX) $(CXXFLAGS) $< -o $@
bin/tests: src/tests.cpp src/cms.hpp src/stream.hpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@
test: bin/tests
	./bin/tests
run: bin/experiments
	./bin/experiments --out results/results.csv --trials 5
quick: bin/experiments
	./bin/experiments --out results/results.csv --trials 2 --scale 0.2
plots:
	python3 scripts/plot.py results/results.csv results/
clean:
	rm -rf bin
