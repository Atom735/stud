#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include <intrin.h>

static HINSTANCE                g_hInstance                 = NULL;
static HWND                     g_hWnd                      = NULL;
static const LPCTSTR            g_cszClassName = TEXT( "WCT: Gilyazeev A.R." );
static LPSTR                    g_szCL                      = NULL;

static HDC                      hWndDC                      = NULL;
static UINT                     nWndWidth                   = 0;
static UINT                     nWndHeight                  = 0;

static HDC                      hBmpDC                      = NULL;
static HBITMAP                  hBmp                        = NULL;
static PBYTE                    pBmpBuf                     = NULL;
static UINT                     nBmpWidth                   = 0;
static UINT                     nBmpHeight                  = 0;

static LPCSTR                   szFileName                  = "_data.bin";
static FILE                    *pF                          = NULL;

static UINT32                   kN_GridX                    = 256;

static double                   kR_Alpha                    = 2.0;
static double                   kR_K_Mu                     = 1e-1;
static double                   kR_dX;
static double                   kR_1_dX;
static double                   kR_dT                       = 1e-3;
static double                   kR_dT_dX;
static double                   kR_S_t                      = 1.0;
static double                   kR_S_x                      = 0.0;

static double                  *pMemPtr                     = NULL;
static double                  *pR_S                        = NULL;
static double                  *pR_Phi                      = NULL;
static double                  *pR_f                        = NULL;
static double                  *pR_P                        = NULL;
static double                  *pR_U                        = NULL;
static double                  *pR_1_Sum_Phi                = NULL;

static double                  *pRG_S                       = NULL;
static double                  *pRG_Phi                     = NULL;
static double                  *pRG_f                       = NULL;
static double                  *pRG_P                       = NULL;
static double                  *pRG_U                       = NULL;

static double                   fR_P_Err;
static UINT32                   nN_P_I;
static UINT32                   nN_S_I;
static UINT                     nN_P_II;

static HBRUSH                   hBrush_S_fg                 = NULL;
static HBRUSH                   hBrush_S_bg                 = NULL;
static HBRUSH                   hBrush_Phi_fg               = NULL;
static HBRUSH                   hBrush_Phi_bg               = NULL;
static HBRUSH                   hBrush_f_fg                 = NULL;
static HBRUSH                   hBrush_f_bg                 = NULL;
static HBRUSH                   hBrush_P_fg                 = NULL;
static HBRUSH                   hBrush_P_bg                 = NULL;
static HBRUSH                   hBrush_U_fg                 = NULL;
static HBRUSH                   hBrush_U_bg                 = NULL;

static const UINT               kN_HeaderSize               =
                                sizeof(kN_GridX) + sizeof(nN_S_I) +
                                sizeof(kR_Alpha) + sizeof(kR_K_Mu) +
                                sizeof(kR_dT) + sizeof(kR_S_t) + sizeof(kR_S_x);

static BOOL                     bFlag_Dbg                   = FALSE;
static BOOL                     bFlag_Graph                 = FALSE;
static BOOL                     bFlag_FileRewrite           = FALSE;


static VOID rSolutionFile_HeaderWrite ( )
{
  if ( pF == NULL ) return;
  fseek ( pF , 0, SEEK_SET );
  #define D_write(a) fwrite ( &a, 1, sizeof(a), pF )
  D_write ( kN_GridX );
  D_write ( nN_S_I );
  D_write ( kR_Alpha );
  D_write ( kR_K_Mu );
  D_write ( kR_dT );
  D_write ( kR_S_t );
  D_write ( kR_S_x );
  #undef D_write

  if ( !bFlag_Dbg ) return;

  const UINT32 kN_FrameSize = sizeof(double) * kN_GridX * 2;
  FILE * const pFd = fopen ( "_writed.txt", "ab" );
  #define D_write_U(a) fprintf ( pFd, "%-10.10s (% 2d) = %d\n", #a, sizeof(a), a );
  #define D_write_D(a) fprintf ( pFd, "%-10.10s (% 2d) = %f\n", #a, sizeof(a), a );
  D_write_U ( kN_HeaderSize );
  D_write_U ( kN_FrameSize );
  D_write_U ( kN_GridX );
  D_write_U ( nN_S_I );
  D_write_D ( kR_Alpha );
  D_write_D ( kR_K_Mu );
  D_write_D ( kR_dT );
  D_write_D ( kR_S_t );
  D_write_D ( kR_S_x );
  #undef D_write_U
  #undef D_write_D
  fprintf ( pFd, "==============================\n" );
  fclose ( pFd );
}
static VOID rSolutionFile_HeaderRead ( )
{
  if ( pF == NULL ) return;
  fseek ( pF , 0, SEEK_SET );
  #define D_read(a) fread ( &a, 1, sizeof(a), pF )
  D_read ( kN_GridX );
  D_read ( nN_S_I );
  D_read ( kR_Alpha );
  D_read ( kR_K_Mu );
  D_read ( kR_dT );
  D_read ( kR_S_t );
  D_read ( kR_S_x );
  #undef D_read

  if ( !bFlag_Dbg ) return;

  const UINT32 kN_FrameSize = sizeof(double) * kN_GridX * 2;
  FILE * const pFd = fopen ( "_readed.txt", "ab" );
  #define D_write_U(a) fprintf ( pFd, "%-10.10s (% 2d) = %d\n", #a, sizeof(a), a );
  #define D_write_D(a) fprintf ( pFd, "%-10.10s (% 2d) = %f\n", #a, sizeof(a), a );
  D_write_U ( kN_HeaderSize );
  D_write_U ( kN_FrameSize );
  D_write_U ( kN_GridX );
  D_write_U ( nN_S_I );
  D_write_D ( kR_Alpha );
  D_write_D ( kR_K_Mu );
  D_write_D ( kR_dT );
  D_write_D ( kR_S_t );
  D_write_D ( kR_S_x );
  #undef D_write_U
  #undef D_write_D
  fprintf ( pFd, "==============================\n" );
  fclose ( pFd );
}
static VOID rSolutionFile_FrameAdd ( )
{
  if ( pF == NULL ) return;
  const UINT nSz = sizeof(double) * kN_GridX;
  fseek ( pF , 0, SEEK_END );
  fwrite ( pR_S, 1, nSz, pF );
  fwrite ( pR_P, 1, nSz, pF );
}
static VOID rSolutionFile_FrameRead ( const UINT i )
{
  if ( pF == NULL ) return;
  const UINT nSz = sizeof(double) * kN_GridX;
  fseek ( pF , kN_HeaderSize + ( nSz * i * 2 ), SEEK_SET );
  fread ( pR_S, 1, nSz, pF );
  fread ( pR_P, 1, nSz, pF );
}

static VOID rSolve_f ( )
{
  for ( UINT i = 0; i < kN_GridX-1; ++i )
  {
    pR_f [ i ] = pow ( pR_S [ i ], kR_Alpha );
  }
}
static VOID rSolve_Phi ( )
{
  for ( UINT i = 0; i < kN_GridX-1; ++i )
  {
    pR_Phi [ i ] = pR_f [ i ] + kR_K_Mu * pow ( 1.0 - pR_S [ i ], kR_Alpha );
  }
  for ( UINT i = 1; i < kN_GridX-1; ++i )
  {
    pR_1_Sum_Phi [ i ] = 1.0 / ( pR_Phi [ i-1 ] + pR_Phi [ i ] );
  }
}
static VOID rSolve_P ( )
{
  double fR_P_Err_Old = fR_P_Err;
  fR_P_Err = 0.0;
  double fR_P_Buf;

  for ( UINT i = 1; i < kN_GridX-1; ++i )
  {
    /* Old
    fR_P_Buf = ( pR_Phi [ i-1 ] * pR_P [ i-1 ] + pR_Phi [ i ] * pR_P [ i+1 ] ) /
            ( pR_Phi [ i-1 ] + pR_Phi [ i ] );
    */
    fR_P_Buf = ( pR_Phi [ i-1 ] * pR_P [ i-1 ] + pR_Phi [ i ] * pR_P [ i+1 ] ) *
            pR_1_Sum_Phi [ i ];
    fR_P_Err += fabs ( fR_P_Buf - pR_P [ i ] );
    pR_P [ i ] = fR_P_Buf;
  }
  ++nN_P_I;

  if ( fR_P_Err_Old == fR_P_Err )
  {
    ++nN_P_II;
  }
  else
  {
    nN_P_II = 0;
  }
}
static VOID rSolve_U ( )
{
  for ( UINT i = 0; i < kN_GridX-1; ++i )
  {
    pR_U [ i ] = kR_1_dX * pR_f [ i ] * ( pR_P [ i ] - pR_P [ i+1 ] );
  }
}
static VOID rSolve_S ( )
{
  for ( UINT i = 1; i < kN_GridX-1; ++i )
  {
    pR_S [ i ] += kR_dT_dX * ( pR_U [ i-1 ] - pR_U [ i ] );
  }
  nN_P_I = 0;
}

static VOID rOnCreate ( )
{
  hBrush_S_fg   = CreateSolidBrush ( RGB ( 0x1f, 0x7f, 0xff ) );
  hBrush_S_bg   = CreateSolidBrush ( RGB ( 0x1f, 0x17, 0x0f ) );
  hBrush_Phi_fg = CreateSolidBrush ( RGB ( 0x7f, 0x7f, 0x7f ) );
  hBrush_Phi_bg = CreateSolidBrush ( RGB ( 0x1f, 0x1f, 0x1f ) );
  hBrush_f_fg   = CreateSolidBrush ( RGB ( 0x7f, 0x7f, 0x7f ) );
  hBrush_f_bg   = CreateSolidBrush ( RGB ( 0x1f, 0x1f, 0x1f ) );
  hBrush_P_fg   = CreateSolidBrush ( RGB ( 0x9f, 0x9f, 0x9f ) );
  hBrush_P_bg   = CreateSolidBrush ( RGB ( 0x3f, 0x3f, 0x3f ) );
  hBrush_U_fg   = CreateSolidBrush ( RGB ( 0x7f, 0xff, 0x7f ) );
  hBrush_U_bg   = CreateSolidBrush ( RGB ( 0x1f, 0x1f, 0x1f ) );

  nN_P_I = 0;
  nN_S_I = 0;
  fR_P_Err = 0.0;


  VOID _malloc ( )
  {
    pMemPtr = _mm_malloc ( sizeof(double) * kN_GridX * 16, 8 * sizeof(double) );
    pR_S    = pMemPtr + kN_GridX * (0+0);
    pR_Phi  = pMemPtr + kN_GridX * (1+0);
    pR_f    = pMemPtr + kN_GridX * (2+0);
    pR_P    = pMemPtr + kN_GridX * (3+0);
    pR_U    = pMemPtr + kN_GridX * (4+0);
    pR_1_Sum_Phi = pMemPtr + kN_GridX * (5+0);

    pRG_S   = pMemPtr + kN_GridX * (0+8);
    pRG_Phi = pMemPtr + kN_GridX * (1+8);
    pRG_f   = pMemPtr + kN_GridX * (2+8);
    pRG_P   = pMemPtr + kN_GridX * (3+8);
    pRG_U   = pMemPtr + kN_GridX * (4+8);

    kR_1_dX = ((double)(kN_GridX-1));
    kR_dX = 1.0 / kR_1_dX;
    kR_dT_dX = kR_dT * kR_1_dX;
  }

  if ( !bFlag_FileRewrite )
  {
    pF = fopen ( szFileName, "r+b" );
  }
  if ( pF == NULL )
  {
    pF = fopen ( szFileName, "wb" );
    rSolutionFile_HeaderWrite ( );

    _malloc ( );

    for ( UINT i = 0; i < kN_GridX; ++i )
    {
      pR_S [ i ] = kR_S_x;
    }
    pR_S [ 0 ] = kR_S_t;

    for ( UINT i = 0; i < kN_GridX; ++i )
    {
      pR_P [ i ] = 1.0 - ( ((double)i) * kR_dX );
    }

    fclose ( pF );
    pF = fopen ( szFileName, "r+b" );
  }
  else
  {
    rSolutionFile_HeaderRead ( );

    _malloc ( );

    if ( nN_S_I == 0 )
    {
      for ( UINT i = 0; i < kN_GridX; ++i )
      {
        pR_S [ i ] = kR_S_x;
      }
      pR_S [ 0 ] = kR_S_t;

      for ( UINT i = 0; i < kN_GridX; ++i )
      {
        pR_P [ i ] = 1.0 - ( ((double)i) * kR_dX );
      }
    }
    else
    {
      rSolutionFile_FrameRead ( nN_S_I-1 );
    }

  }

  rSolve_f ( );
  rSolve_Phi ( );
}

static VOID rOnDestroy ( )
{
  rSolutionFile_HeaderWrite ( );

  if ( pF )
  {
    fclose ( pF );
  }

  _mm_free ( pMemPtr );
  pMemPtr = NULL;

  pR_S    = NULL;
  pR_Phi  = NULL;
  pR_f    = NULL;
  pR_P    = NULL;
  pR_U    = NULL;

  pRG_S   = NULL;
  pRG_Phi = NULL;
  pRG_f   = NULL;
  pRG_P   = NULL;
  pRG_U   = NULL;
}

static VOID rOnIdle ( )
{
  rSolve_P ( );
  if ( nN_P_II > 0x100 || fR_P_Err == 0.0 )
  {
    ++nN_S_I;
    rSolutionFile_FrameAdd ( );
    rSolve_U ( );
    rSolve_S ( );
    rSolve_f ( );
    rSolve_Phi ( );
  }
  if ( bFlag_Graph )
  {
    InvalidateRect ( g_hWnd, NULL, FALSE );
  }

  if ( !bFlag_Dbg ) return;

  CHAR buf[512];
  UINT i = 0;
  #define D_printf(...) TextOutA ( hWndDC, 0, 16*(i++), buf, sprintf ( buf, __VA_ARGS__ ) )
  D_printf( "Err: %.40lf", fR_P_Err );
  D_printf( "I_P_II: %08d", nN_P_II );
  D_printf( "I_P: %08d", nN_P_I );
  D_printf( "I_S: %08d", nN_S_I );
  #undef D_printf

}

static VOID rPlot ( const HDC hDC, const RECT rc,
        double const * const pF, const UINT nCount,
        const HBRUSH hBr0, const HBRUSH hBr1,
        const double fNorm_A, const double fNorm_B )
{
  FillRect ( hDC, &rc, hBr0 );
  const UINT kW = rc.right - rc.left;
  const double kH = rc.bottom - rc.top;
  RECT rcL = rc;
  for ( UINT ix = 0; ix < kW; )
  {
    const UINT kIx = ix * nCount / kW;
    rcL.left = ix + rc.left;
    UINT kIW = 1; ++ix;
    while ( ix * nCount / kW == kIx ) { ++kIW; ++ix; }
    rcL.right = ix + rc.left;
    const double fY = pF[kIx] * fNorm_A + fNorm_B;
    rcL.top = rc.bottom - (fY * kH);
    FillRect ( hDC, &rcL, hBr1 );
  }
}

static VOID rOnPaint ( const HDC hDC )
{
  const UINT nH = nWndHeight;
  const UINT nW = nWndWidth;
  RECT rc =
  {
    .left     = 0,
    .right    = nW,
    .top      = 0,
    .bottom   = nH/5,
  };
  if ( nN_P_I <= 5 || bFlag_Graph == 2 )
  {
    rPlot ( hBmpDC, rc, pR_S, kN_GridX, hBrush_S_bg, hBrush_S_fg, 0.5, 0.25 );
    rc.top      = rc.bottom;
    rc.bottom   = nH*2/5;
    rPlot ( hBmpDC, rc, pR_Phi, kN_GridX, hBrush_Phi_bg, hBrush_Phi_fg, 0.5, 0.25 );
    rc.top      = rc.bottom;
    rc.bottom   = nH*3/5;
    rPlot ( hBmpDC, rc, pR_f, kN_GridX, hBrush_f_bg, hBrush_f_fg, 0.5, 0.25 );
    rc.top      = rc.bottom;
    rc.bottom   = nH*4/5;
    rPlot ( hBmpDC, rc, pR_P, kN_GridX, hBrush_P_bg, hBrush_P_fg, 0.5, 0.25 );
    rc.top      = rc.bottom;
    rc.bottom   = nH;
    rPlot ( hBmpDC, rc, pR_U, kN_GridX, hBrush_U_bg, hBrush_U_fg, 0.5, 0.25 );

    BitBlt ( hDC, 0, 0, nWndWidth, nWndHeight, hBmpDC, 0, 0, SRCCOPY );
  }

}

LRESULT CALLBACK WndProc ( HWND hWnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam )
{
  switch ( uMsg )
  {
    case WM_CREATE:
    {
      // hWndDC = GetWindowDC ( hWnd );
      hWndDC = GetDC ( hWnd );
      assert ( hWndDC );
      hBmpDC = CreateCompatibleDC ( hWndDC );
      assert ( hBmpDC );

      rOnCreate ( );
      return 0;
    }
    case WM_SIZE:
    {
        nWndWidth  = LOWORD(lParam);
        nWndHeight = HIWORD(lParam);
        if ( ( nWndWidth > nBmpWidth ) || ( nWndHeight > nBmpHeight ) )
        {
          if ( nWndWidth > nBmpWidth )
          {
            nBmpWidth = ( nWndWidth + 0xf ) & 0xfffffff0;
          }
          if ( nWndHeight > nBmpHeight )
          {
            nBmpHeight = ( nWndHeight + 0xf ) & 0xfffffff0;
          }
          if ( hBmp ) { DeleteObject ( hBmp ); }
          BITMAPINFO bi =
          {
            .bmiHeader.biSize           = sizeof(bi.bmiHeader),
            .bmiHeader.biWidth          = nBmpWidth,
            // .bmiHeader.biHeight         = nBmpHeight,
            .bmiHeader.biHeight         = -((INT)nBmpHeight),
            .bmiHeader.biPlanes         = 1,
            .bmiHeader.biBitCount       = 32,
            .bmiHeader.biCompression    = BI_RGB,
            .bmiHeader.biSizeImage      = 4*nBmpWidth*nBmpHeight,
            .bmiHeader.biXPelsPerMeter  = 0,
            .bmiHeader.biYPelsPerMeter  = 0,
            .bmiHeader.biClrUsed        = 0,
            .bmiHeader.biClrImportant   = 0,
          };
          hBmp = CreateDIBSection ( hBmpDC, &bi, DIB_RGB_COLORS,
                  ((void**)(&pBmpBuf)), NULL, 0 );
          assert ( hBmp );
          assert ( pBmpBuf );
          PUINT32 pB = ((PUINT32)pBmpBuf);
          for ( UINT iy = 0; iy < nBmpHeight; ++iy )
          {
            for ( UINT ix = 0; ix < nBmpWidth; ++ix )
            {
              *pB = (((iy^ix)>>3)&1)?0x00ff00ff:0x0000ff00;
              ++pB;
            }
          }
          SelectObject ( hBmpDC, hBmp );
        }
        return 0;
    }
    case WM_DESTROY:
    {
      rOnDestroy ( );
      DeleteObject ( hBmp );
      DeleteDC ( hBmpDC );
      ReleaseDC ( hWnd, hWndDC );
      PostQuitMessage ( 0 );
      return 0;
    }
    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hDC = BeginPaint ( hWnd, &ps );
      assert ( hDC );
      if ( bFlag_Graph )
      {
        rOnPaint ( hDC );
      }

      CHAR buf[512];
      UINT i = 8;
      #define D_printf(...) TextOutA ( hDC, 0, 16*(i++), buf, sprintf ( buf, __VA_ARGS__ ) )
      // TextOutA ( hDC, 0, 16*(i++), g_szCL, strlen ( g_szCL ) );
      D_printf ( g_szCL );
      D_printf ( "FileName = %s", szFileName );
      D_printf ( "kN_GridX = %d", kN_GridX );
      D_printf ( "kR_Alpha = %f", kR_Alpha );
      D_printf ( "kR_K_Mu = %f", kR_K_Mu );
      D_printf ( "kR_dT = %f", kR_dT );
      D_printf ( "kR_S_t = %f", kR_S_t );
      D_printf ( "kR_S_x = %f", kR_S_x );

      #undef D_printf


      EndPaint ( hWnd, &ps );
      return 0;
    }
  }
  return DefWindowProc ( hWnd, uMsg, wParam, lParam );
}


INT APIENTRY wWinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance,
        LPWSTR lpCmdLine, INT nShowCmd )
{
  g_szCL = GetCommandLineA ( );
  LPSTR szCL = g_szCL;
  if ( *szCL == '"' )
  {
    ++szCL;
    while ( *szCL != '"' )
    {
      if ( *szCL == 0 ) goto P_BEGIN;
      ++szCL;
    }
    ++szCL;
  }
  else
  {
    while ( *szCL != ' ' )
    {
      if ( *szCL == 0 ) goto P_BEGIN;
      ++szCL;
    }
  }
  while ( *szCL == ' ' )
  {
    ++szCL;
  }
  if ( *szCL == 0 ) goto P_BEGIN;

  while ( *szCL != '-' )
  {
    switch ( *szCL )
    {
      case 0: goto P_BEGIN;
      case 'D': case 'd': bFlag_Dbg                         = TRUE; break;
      case 'V': case 'v': bFlag_Graph                       = TRUE; break;
      case 'N': case 'n': bFlag_FileRewrite                 = TRUE; break;
      case '-':
        {
          #define D_cmp(a) ((memcmp(szCL,a,sizeof(a)-1)==0)&&((szCL+=sizeof(a)-1)))
          if ( D_cmp ("-f=\"" ) || D_cmp ("-file=\"" ) )
          {
            szFileName = szCL;
            while ( *szCL != '\"' )
            {
              if ( *szCL == 0 ) goto P_BEGIN;
              ++szCL;
            }
            *szCL = 0;
            ++szCL;
            continue;
          }
          else
          if ( D_cmp("-f=") || D_cmp("-file=") )
          {
            szFileName = szCL;
            while ( *szCL != ' ' )
            {
              if ( *szCL == 0 ) goto P_BEGIN;
              ++szCL;
            }
            *szCL = 0;
            ++szCL;
            continue;
          }
          else
          if ( D_cmp("-N=") || D_cmp("-kN_GridX=") || D_cmp("-GridX=") )
          {
            kN_GridX = strtoul ( szCL, &szCL, 0 );
            continue;
          }
          else
          if ( D_cmp("-A=") || D_cmp("-kR_Alpha=") || D_cmp("-Alpha=") )
          {
            kR_Alpha = strtod  ( szCL, &szCL );
            continue;
          }
          else
          if ( D_cmp("-K=") || D_cmp("-kR_K_Mu=") || D_cmp("-K_Mu=") )
          {
            kR_K_Mu = strtod  ( szCL, &szCL );
            continue;
          }
          else
          if ( D_cmp("-T=") || D_cmp("-kR_dT=") || D_cmp("-dT=") )
          {
            kR_dT = strtod  ( szCL, &szCL );
            continue;
          }
          else
          if ( D_cmp("-St=") || D_cmp("-kR_S_t=") || D_cmp("-S_t=") )
          {
            kR_S_t = strtod  ( szCL, &szCL );
            continue;
          }
          else
          if ( D_cmp("-Sx=") || D_cmp("-kR_S_x=") || D_cmp("-S_x=") )
          {
            kR_S_x = strtod  ( szCL, &szCL );
            continue;
          }
          #undef D_cmp
        }
        continue;
    }
    ++szCL;
  }

  P_BEGIN:

  g_hInstance = hInstance;
  {
    WNDCLASSEX wc = {
      .cbSize        = sizeof ( WNDCLASSEXW ),
      .style         = CS_VREDRAW|CS_HREDRAW|CS_OWNDC,
      .lpfnWndProc   = WndProc,
      .cbClsExtra    = 0,
      .cbWndExtra    = 0,
      .hInstance     = g_hInstance,
      .hIcon         = NULL,
      .hCursor       = NULL,
      .hbrBackground = NULL,
      .lpszMenuName  = NULL,
      .lpszClassName = g_cszClassName,
      .hIconSm       = NULL,
    };
    RegisterClassEx ( &wc );
  }
  g_hWnd = CreateWindowEx ( 0, g_cszClassName, NULL,
          WS_OVERLAPPEDWINDOW | WS_VISIBLE,
          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
          NULL, NULL, g_hInstance, NULL );
  MSG msg;
  while ( TRUE )
  {
    while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
    {
      if ( msg.message == WM_QUIT ) { goto P_END; }
      TranslateMessage ( &msg );
      DispatchMessage ( &msg );
    }
    rOnIdle ( );
  }
  P_END: return msg.wParam;
}
