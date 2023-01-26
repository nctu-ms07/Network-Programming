#! /bin/sh

vimdiff -c "set diffopt+=iwhite" output/$1.txt answer/$1.txt
