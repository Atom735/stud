.PHONY : all

# PATH_GCC := C:\\a\\mingw64\\bin\\

CC := gcc.exe


CPPFLAGS :=\
	-DWIN32_LEAN_AND_MEAN\
	-D_WIN32_WINNT=_WIN32_WINNT_WIN7\
	-DUNICODE\
	-DNDEBUG\

CFLAGS :=\
	-Wall\
	-O3\
	-msse2\
	-m32 -mfpmath=sse -Ofast -flto -march=native -funroll-loops\
	-municode\
# 	-mwindows\

LDFLAGS :=\
	-Wall\
	-O3\
	-msse2\
	-m32 -mfpmath=sse -Ofast -flto -march=native -funroll-loops\
  -municode\
	-lGDI32\
	-lOpenGL32\

all : main.exe
	main.exe

main.exe : src/main.c
	$(CC) -o $@ $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -O3

