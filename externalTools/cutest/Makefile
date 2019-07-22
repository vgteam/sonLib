include ../../include.mk
binPath = ../../bin
libPath = ../../lib

all : ${libPath}/cuTest.a

clean : 
#Removing test binaries
	rm -f ${libPath}/cuTest.a ${libPath}/CuTest.h

${libPath}/cuTest.a : CuTest.c CuTest.h
	@mkdir -p $(dir $@)
	${cxx} ${cflags} -c CuTest.c
	ar rc cuTest.a *.o
	ranlib cuTest.a
	rm -f *.o
	cat cuTest.a >${libPath}/cuTest.a
	cat CuTest.h >${libPath}/CuTest.h
