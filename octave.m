
fid = fopen ( "Z:\\projects\\stud-master\\_test0.bin", "rb" );
fseek ( fid, 72 );
y = fread ( fid, [128, Inf], "double" );
plot(y(:,1:100:2003))
