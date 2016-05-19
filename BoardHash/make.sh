#!/bin/sh
clang -s -flto -mmacosx-version-min=10.9 -Os BoardHash.c -o BoardHash