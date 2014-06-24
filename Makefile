OPT=-O3

all: pfolioUZK solvers

pfolioUZK: pfolioUZK.cc
	g++ $(OPT) $(CXXFLAGS) -o pfolioUZK  pfolioUZK.cc -lboost_program_options -static 

solvers:
	./src/buildSolvers.sh

clean:
	rm -f pfolioUZK bin/satUZK bin/satUZK_wrapper bin/glucose_static bin/glucose_wrapper \
	bin/plingeling bin/lingeling bin/TNM bin/march_hi bin/contrasat bin/MPhaseSAT_M \
	bin/sparrow2011 bin/*.sh bin/SatELite_release
	rm -rf src/satUZK src/glucose_2.0 src/lingeling-587f-4882048-110513 src/TNM src/march_hi \
	src/Minisat-2.2.0-hack-contrasat src/MPhaseSAT_M src/sparrow2011 src/satelite

clean-src:
	rm -rf src/satUZK src/glucose_2.0 src/lingeling-587f-4882048-110513 src/TNM src/march_hi \
        src/Minisat-2.2.0-hack-contrasat src/MPhaseSAT_M src/sparrow2011 src/satelite

