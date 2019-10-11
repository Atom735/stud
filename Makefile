.PHONY : all

# ifeq ( $(PROCESSOR_ARCHITECTURE), AMD64 )
# 	MFLAGS =
#
# endif

CC := gcc.exe


CPPFLAGS :=\
	-DWIN32_LEAN_AND_MEAN\
	-D_WIN32_WINNT=_WIN32_WINNT_WIN7\
	-DUNICODE\
	-DNDEBUG\

CFLAGS :=\
	-Wall\
	$(MFLAGS)\
	-municode\
# 	-mwindows\

LDFLAGS :=\
	-Wall\
	$(MFLAGS)\
  -municode\
	-lGDI32\
	-lOpenGL32\
	-lShell32\

all : main.exe
	main \
	-G -N 128 -a 1.0 -Km 1e-2 -S_t 1.0 -S_x 0.0 -dT 1e-3 -IS 100 -txt data/d1.txt data/d1.bin \
	-G -N 128 -a 2.0 -Km 1e-2 -S_t 1.0 -S_x 0.0 -dT 1e-3 -IS 100 -txt data/d2.txt data/d2.bin \
	-G -N 128 -a 3.0 -Km 1e-2 -S_t 1.0 -S_x 0.0 -dT 1e-3 -IS 100 -txt data/d3.txt data/d3.bin \
	-G -N 128 -a 4.0 -Km 1e-2 -S_t 1.0 -S_x 0.0 -dT 1e-3 -IS 100 -txt data/d3.txt data/d4.bin \


main.exe : src/main.c
	$(CC) -o $@ $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -O3

