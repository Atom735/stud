.PHONY : all

# PATH_GCC := C:\\a\\mingw64\\bin\\

GCC_DEFINES :=\
	WIN32_LEAN_AND_MEAN\
	_WIN32_WINNT=_WIN32_WINNT_WIN7\
	UNICODE\
	NDEBUG\


CC := gcc.exe

CPPFLAGS :=\
	$(addprefix -D, $(GCC_DEFINES))\

CFLAGS :=\
	-Wall\
	-g\
	-msse2\
	-O3\
	-m32 -mfpmath=sse -Ofast -flto -march=native -funroll-loops\
#  	-mwindows -municode\

LDFLAGS :=\
	-g\
	-msse2\
	-O3\
	-m32 -mfpmath=sse -Ofast -flto -march=native -funroll-loops\
# 	-mwindows -municode\


FILES :=\
	main.c\

SOURCES := $(addprefix src/, $(FILES))

all : main.exe
	main.exe\
		-new -N 128 -IIM 16 -a 3.0 -Km 0.1 -dT 0.01 -F 1000 -S_t 1.0 -S_x 0.0 -txt _test0.txt\
		_test0.bin


main.exe : $(SOURCES)
	$(CC) -o $@ $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -O3

