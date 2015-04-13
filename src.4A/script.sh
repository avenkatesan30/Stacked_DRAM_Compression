#!/bin/sh
Files=../traces/*.gz

for j in $Files
do
echo "Working on $j"
echo "\n"
./sim -mode 1 $j > $j.out
done
