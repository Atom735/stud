[fd, err] = mopen ( "Z:\\projects\\stud-master\\_test0.bin" );
head_u = mget( 6, 'ui', fd );
head_d = mget( 6, 'd', fd );
kN_FrameCount = head_u(2);
kN_GridX = head_u(3);
 
fS(1,:) = mget( kN_GridX, 'd', fd );
for i = 1:kN_FrameCount    
    fP(i,:) = mget( kN_GridX, 'd', fd );
    fS(i+1,:) = mget( kN_GridX, 'd', fd );
end;

mclose ( fd );


clf()
gca().background = color(0.3,0.3,0.3);
subplot(2,1,1)

for i = 1:100:kN_FrameCount    
    plot(0:1/(kN_GridX-1):1,fP(i,:),"foreground",hsv2rgb(i/kN_FrameCount,1,1));
end;
title( "P(x)", "fontsize",3)
gca().background = color(0.3,0.3,0.3);

subplot(2,1,2)
for i = 1:100:kN_FrameCount    
    plot(0:1/(kN_GridX-1):1,fS(i,:),"foreground",hsv2rgb(i/kN_FrameCount,1,1));
end;
title( "S(x)", "fontsize",3)
gca().background = color(0.3,0.3,0.3);

plot3d(0:1/(kN_FrameCount-1):1,0:1/(kN_GridX-1):1,fP);
plot3d(0:1/(kN_FrameCount):1,0:1/(kN_GridX-1):1,fS);
