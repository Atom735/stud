szPathToData            = 'C:\ARGilyazeev\github\stud\data\';


function rDataPlot ( szFileName )
  [fdData, err]           = mopen ( szPathToData + szFileName );
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

  gca().background = color(0.3,0.3,0.3);
  for i = 1:10:kN_FrameCount
      plot ( listX, rDataLoadFrame(i), "foreground",hsv2rgb(i/kN_FrameCount,1,1));
  end;
endfunction

rDataPlot('N9_4_100_30_0.bin');
rDataPlot('N9_4_102_30_0.bin');
rDataPlot('N9_4_104_30_0.bin');
