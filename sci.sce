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


  listX = 0:kR_dX:1;


endfunction


function [ s, p ] = rDataLoadFrame ( i )
  mseek ( 72 + i*kN_GridX*2*8, fdData );
  s = mget( kN_GridX, 'd', fdData );
  if i < kN_FrameCount then
    p = mget( kN_GridX, 'd', fdData );
  end;
endfunction

plot ( listX, rDataLoadFrame(10) );




