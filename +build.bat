rem "mingw32-make.exe"
gcc.exe^
  -o main.exe^
  -Wall -g^
  -DWIN32_LEAN_AND_MEAN^
  -D_WIN32_WINNT=_WIN32_WINNT_WIN7^
  -DUNICODE^
  src/main.c^
  -mwindows -municode -m64

main