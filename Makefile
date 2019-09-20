.PHONY : all clean

PATH_GCC := C:\\a\\mingw64\\bin\\

GCC_DEFINES :=\
	WIN32_LEAN_AND_MEAN\
	_WIN32_WINNT=_WIN32_WINNT_WIN7\
	UNICODE\


CC := $(PATH_GCC)\\gcc.exe

CPPFLAGS :=\
	$(addprefix -D, $(GCC_DEFINES))\

CFLAGS :=\
	-Wall\
	-g\

LDFLAGS :=\
	-g\


FILES :=\
	main.c\

SOURCES := $(addprefix src/, $(FILES))
OBJECTS := $(addsuffix .o, $(addprefix obj/, $(FILES)))

all : main.exe
	main.exe


obj :
	MD obj

clean :
	DEL /S obj

main.exe : $(SOURCES)
	$(CC) -o $@ $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS)

