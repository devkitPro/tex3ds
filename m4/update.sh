#!/bin/bash

for i in *.m4
do
	wget "http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/$i" -O $i
done
