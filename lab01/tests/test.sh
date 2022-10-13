#!/bin/bash
cd .. && make clean && make && echo
for i in tests/input/*
do
    echo "Testing $i: echo..."
    cmp --silent $i <(./list_harness $i "echo") && echo '### PASSED! ###' || echo '~~~ FAILED! ~~~'

    echo "Testing $i: tail..."
    cmp --silent $i <(./list_harness $i "tail") && echo '### PASSED! ###' || echo '~~~ FAILED! ~~~'

    echo "Testing $i: tail-remove..."
    cmp --silent tests/output/$(basename $i) <(./list_harness $i "tail-remove") && echo '### PASSED! ###' || echo '~~~ FAILED! ~~~'

    echo
done
