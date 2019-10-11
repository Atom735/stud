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

all : main.exe
	$(MFLAGS)
	main.exe

main.exe : src/main.c
	$(CC) -o $@ $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -O3

