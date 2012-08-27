#!/usr/local/bin/bash

for i in *.c; do
	echo "Compiling $i...";
	`cc -c -nostdinc -I-  -I. -I/usr/src/sys -I/usr/src/sys/dev -I/usr/src/sys/contrib/dev/acpica -I/usr/src/sys/contrib/ipfilter -I/usr/obj/usr/src/sys/GENERIC -D_KERNEL -include opt_global.h -fno-common  -mno-align-long-strings -mpreferred-stack-boundary=2 -ffreestanding -Werror -o obj/$i.o $i`;
done	
