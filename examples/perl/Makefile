CFLAGS := -m64 -g -fPIC -Dbool=char -I/usr/lib/perl/5.18.2/CORE -I. -DLINUX=2 -D_REENTRANT -D_LARGEFILE64_SOURCE -shared

MODULE := fl

all: $(MODULE).so
	LD_LIBRARY_PATH=../../lin.x64/rel ./try.pl

debug: $(MODULE).so
	printf "break $(MODULE)_wrap.c:2118\nrun\nwhere\nquit\n" | LD_LIBRARY_PATH=../../lin.x64/rel gdb -q --args perl ./try.pl

$(MODULE).so: $(MODULE)_wrap.c
	gcc $(CFLAGS) \
		-I/home/chris/makestuff/common \
		-I../.. \
		$< -o $@ \
		-L/home/chris/makestuff/libs/liberror/lin.x64/rel -lerror \
		-L../../lin.x64/rel -lfpgalink
	cat extra.pm >> $(MODULE).pm

#		-I/home/chris/makestuff/libs/liberror \

#DEBUG := -debug-tmsearch

$(MODULE)_wrap.c: $(MODULE).i
	swig2.0 -perl5 $(DEBUG) $<

clean: FORCE
	rm -f *.so fl.pm *_wrap.c

FORCE:
