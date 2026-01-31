#!/bin/bash

gcc -o pinekicker_test -DUNIT_TEST src/pinekicker_test.c
./pinekicker_test
