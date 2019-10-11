szPathToData            = 'C:\ARGilyazeev\github\stud\data\';
[fdData, err]           = mopen ( szPathToData + 'orig_0.bin' );
kI_Magic                = mget( 1, 'ul', fdData );
kN_FrameCount           = mget( 1, 'u', fdData );
kN_GridX                = mget( 1, 'u', fdData );
kN_ItersIdentMi         = mget( 1, 'u', fdData );
kN_ItersSkips           = mget( 1, 'u', fdData );
kR_ErrorMax             = mget( 1, 'd', fdData );
kR_Alpha                = mget( 1, 'd', fdData );
kR_K_Mu                 = mget( 1, 'd', fdData );
kR_dT                   = mget( 1, 'd', fdData );
kR_S_t                  = mget( 1, 'd', fdData );
kR_S_x                  = mget( 1, 'd', fdData );
kR_dX                   = 1.0 / ( kN_GridX - 1.0 );

function [ s, p ] = rDataLoadFrame ( i )
  mseek ( 72 + i*kN_GridX*2*8, fdData );
  s = mget( kN_GridX, 'd', fdData );
  if i < kN_FrameCount then
    p = mget( kN_GridX, 'd', fdData );
  end;
endfunction

listX = 0:kR_dX:1;

for i = 1:100:1000
    plot ( listX, rDataLoadFrame(i), "foreground",hsv2rgb(i/1000,1,1));
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
