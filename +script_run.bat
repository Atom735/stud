start main -noGUI -new -N 2048 -dT "1e-5" -F 1000000 -S_t "0.9" -S_x "0.1" -a "3.0" -Err "1e-10" -IS 10 -Km "1" -txt "data/original.txt" "data/original.bin"



start main -noGUI -new -G -N 256 -dT "1e-3" -F 10000 -S_t "0.9" -S_x "0.1" -a "3.0" -IIM 4 -IS 10 -Km "1" -txt "data/orig_0.txt" "data/orig_0.bin"
