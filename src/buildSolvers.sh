#!/bin/sh
ROOT=`pwd`

cd src

rm -rf satUZK
tar xzf satUZK.tar.gz
cd satUZK
make static
cp satUZK $ROOT/bin
cd satUZK_wrapper
make
cp satUZK_wrapper $ROOT/bin

cd $ROOT/src
rm -rf lingeling-587f-4882048-110513
tar xzf lingeling-587f-4882048-110513.tar.gz
cd lingeling-587f-4882048-110513
./configure --competition
make
cp plingeling $ROOT/bin
cp lingeling $ROOT/bin

cd $ROOT/src
rm -rf glucose_2.0
tar xzf glucose_2.0.tar.gz
cd glucose_2.0
./build.sh
cp glucose_static $ROOT/bin
cp SatELite_release $ROOT/bin
cd glucose_wrapper
make
cp glucose_wrapper $ROOT/bin

cd $ROOT/src
rm -rf march_hi
unzip -x march_hi.zip
cd march_hi
make
cp march_hi $ROOT/bin

cd $ROOT/src
rm -rf TNM
tar xf TNM.tar.gz
cd TNM
make
cp TNM $ROOT/bin

cd $ROOT/src
rm -rf sparrow2011
tar xf sparrow2011.tar.gz
cd sparrow2011
make 
cp sparrow2011 $ROOT/bin

cd $ROOT/src
rm -rf MPhaseSAT_M
unzip -x MPhaseSAT_M.zip
cd MPhaseSAT_M
make
cp MPhaseSAT_M $ROOT/bin

cd $ROOT/src
rm -rf Minisat-2.2.0-hack-contrasat
tar xf Minisat-2.2.0-hack-contrasat.tar.gz
cd Minisat-2.2.0-hack-contrasat
./build.sh
cp contrasat $ROOT/bin

cd $ROOT/src
rm -rf satelite
unzip satelite.zip
cd satelite
./build.sh
cp SatELite_release $ROOT/bin

cd $ROOT/src/precompiled
cp * $ROOT/bin



