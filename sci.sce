clear
mclose ( 'all' );
szPathToData            = 'C:\ARGilyazeev\github\stud\data\';

previous_driver = driver ( 'PDF' );

function rDataPlot ( szFileName, g )
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

  function [ s, p, v ] = rDataLoadFrame ( i )
    mseek ( 72 + i*(kN_GridX*2+1)*8, fdData );
    s = mget( kN_GridX, 'd', fdData );
    if i < kN_FrameCount then
      p = mget( kN_GridX, 'd', fdData );
      v = mget( 1, 'd', fdData );
    end;
  endfunction
  
  
  xinit ( szPathToData + szFileName + '.S.pdf' );
  xtitle ( '', 'X', 'S' );
    for i = 1:floor(0.2/kR_dT):kN_FrameCount
      plot ( listX, rDataLoadFrame(i) );
    end;
  xend();
  
  xinit ( szPathToData + szFileName + '.U.pdf' );
  xtitle ( '', 't', 'u' );
    listTimes = 0:floor(0.01/kR_dT):kN_FrameCount;
    list_dP = listTimes;
    for i = 1:length(listTimes)
      [ s, p, v ] = rDataLoadFrame(listTimes(i));
      list_dP(i) = v;
    end;
    plot ( listTimes*kR_dT, list_dP );
  xend();

endfunction

rDataPlot ( 'St.bin', 0 );
rDataPlot ( 'Sx.bin', 0 );
rDataPlot ( 'S2.bin', 0 );
rDataPlot ( 'original.bin', 0 );

driver(previous_driver);


mclose ( 'all' );



