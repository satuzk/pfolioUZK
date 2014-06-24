#!/bin/bash

# number of all clauses
C=`head -100 $1 | grep "p " | sed -r "s/p[ ]+cnf[ ]+[0-9]+[ ]+//g"`
#echo "Number of clauses: $C"
if [ -z "$C" ]; then
  exit 1
fi
if [ $C -gt 12000000 ]; then
  exit 0
fi
exit 1
