clear
mclose ( 'all' );
szPathToData            = 'C:\ARGilyazeev\github\stud\data\';

function rDataPlot ( szFileName, n, c )
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

  gca().background = color(0.0,0.0,0.0);
  for i = 1:n:kN_FrameCount
      plot ( listX, rDataLoadFrame(i), "foreground",hsv2rgb(i/kN_FrameCount,1,1));
      // plot ( listX, rDataLoadFrame(i), "foreground", c );
  end;


  mclose ( fdData );
endfunction

a = 10;
//rDataPlot ( '_original.bin', a, color(255.0,255.0,255.0) );
//rDataPlot ( '_leftrules.bin', a, color(0.0,0.0,255.0) );
rDataPlot ( '_firstrules.bin', a, color(0.0,255.0,0.0) );



