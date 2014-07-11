#
# makefile 学习
#
demo1.out:say.o demo1.o
	cc -o demo1.out say.o demo1.o
say.o:say.h say.c
	cc -c say.c
demo1.o:demo1.h demo1.c
	cc -c demo1.c
clear:
	rm  demo1.out demo1.o say.o
