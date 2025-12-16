cp src/* tmp/
cp include/* tmp/ 
clear && cppcheck --platform=avr8 --enable=all --inconclusive --suppress=missingIncludeSystem --suppress=unmatchedSuppression -I /path/to/avr8/include/avr -I /path/to/avr8/include tmp/
