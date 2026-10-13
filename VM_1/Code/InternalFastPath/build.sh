#!/bin/bash

gcc -O2 -o producer producer.c -lpthread
gcc -O2 -o consumer consumer.c -lpthread

echo "Build done."
