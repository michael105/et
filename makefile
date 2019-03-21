# A template makefile 
# Set Prog to the program's name
# unpack minilib to the "source dir/minilib"
# or set a link
# copy this makefile into the source dir
# done.


PROG=et

# where to build object files
#BUILDDIR=.

# Don't create obj files, include evrything in one gcc run.
SINGLERUN=1 

NOINCLUDE=1

# GCC
GCC=gcc

# LD
#LD=ld


ifdef with-minilib
include minilib/makefile.include 
endif

#Compile with minilib or
default: 
	$(if $(wildcard minilib/makefile.include), make with-minilib=1, make native )


# make native 
# compile with dynamic loading, os native libs
native: $(PROG).c
	$(info Building dynamic, linked to host\'s libc )
	$(info call "make getminilib" to fetch and extract the "minilib" and compile $(PROG) static (recommended) )
	gcc -O1 -o $(PROG) $(PROG).c


ifndef with-minilib

rebuild:
	make clean
	make

clean:
	$(shell rm $(PROG))
	$(shell cd build && rm *.o)

endif

getminilib: minilib/minilib.h

minilib/minilib.h:
	$(info get minilib)
	git clone https://github.com/michael105/minilib.git minilib || (( (curl https://codeload.github.com/michael105/minilib/zip/master > minilib.zip) || (wget https://codeload.github.com/michael105/minilib/zip/master)) && unzip minilib.zip && mv minilib-master minilib)
	make rebuild

install: $(PROG)
	cp $(PROG) /usr/local/bin

