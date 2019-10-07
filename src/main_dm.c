#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include <intrin.h>

static HINSTANCE                g_hInstance                 = NULL;
static HWND                     g_hWnd                      = NULL;
static const LPCTSTR            g_cszClassName = TEXT( "WCT: Gilyazeev A.R." );

static HDC                      hWndDC                      = NULL;
static UINT                     nWndWidth                   = 0;
static UINT                     nWndHeight                  = 0;

static HDC                      hBmpDC                      = NULL;
static HBITMAP                  hBmp                        = NULL;
static PBYTE                    pBmpBuf                     = NULL;
static UINT                     nBmpWidth                   = 0;
static UINT                     nBmpHeight                  = 0;

static LPCSTR                   szFileName                  = "data.bin";
static FILE                    *pF                          = NULL;

static UINT                     kN_GridX                    = 256;

static double                   kR_Alpha                    = 2.0;
static double                   kR_K_Mu                     = 1e-1;
static double                   kR_dX;
static double                   kR_1_dX;
static double                   kR_dT                       = 1e-3;
static double                   kR_dT_dX;

static double                  *pMemPtr                     = NULL;
static double                  *pR_S                        = NULL;
static double                  *pR_Phi                      = NULL;
static double                  *pR_f                        = NULL;
static double                  *pR_P                        = NULL;
static double                  *pR_U                        = NULL;

static double                  *pRG_S                       = NULL;
static double                  *pRG_Phi                     = NULL;
static double                  *pRG_f                       = NULL;
static double                  *pRG_P                       = NULL;
static double                  *pRG_U                       = NULL;

static double                   fR_P_Err;
static UINT                     nN_P_Iteration;
static UINT                     nN_S_Iteration;
static UINT                     nN_S_IterationCurrent;

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
                                sizeof(kN_GridX) + sizeof(nN_S_Iteration) +
                                sizeof(kR_Alpha) + sizeof(kR_K_Mu) + sizeof(kR_dT);



static VOID rSolutionFile_HeaderWrite ( )
{
  if ( pF == NULL ) return;
  fseek ( pF , 0, SEEK_SET );
  #define _wrtie(a) fwrite ( &a, 1, sizeof(a), pF )
  _wrtie ( kN_GridX );
  _wrtie ( nN_S_Iteration );
  _wrtie ( kR_Alpha );
  _wrtie ( kR_K_Mu );
  _wrtie ( kR_dT );
}
static VOID rSolutionFile_HeaderRead ( )
{
  if ( pF == NULL ) return;
  fseek ( pF , 0, SEEK_SET );
  #define _read(a) fread ( &a, 1, sizeof(a), pF )
  _read ( kN_GridX );
  _read ( nN_S_Iteration );
  _read ( kR_Alpha );
  _read ( kR_K_Mu );
  _read ( kR_dT );
}
static VOID rSolutionFile_FrameAdd ( )
{
  if ( pF == NULL ) return;
  const UINT _Sz = sizeof(double) * kN_GridX;
  fseek ( pF , 0, SEEK_END );
  fwrite ( pR_S, 1, _Sz, pF );
  fwrite ( pR_P, 1, _Sz, pF );
}
static VOID rSolutionFile_FrameRead ( const UINT i )
{
  if ( pF == NULL ) return;
  const UINT _Sz = sizeof(double) * kN_GridX;
  fseek ( pF , kN_HeaderSize + ( _Sz * i ), SEEK_SET );
  fread ( pR_S, 1, _Sz, pF );
  fread ( pR_P, 1, _Sz, pF );
}

static double rR_S_x ( const double x )
{
  return 0.0;
}

static double rR_S_t ( const double t )
{
  return 1.0;
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
}
static VOID rSolve_P ( )
{
  fR_P_Err = 0.0;
  double fR_P_Buf;

  for ( UINT i = 1; i < kN_GridX-1; ++i )
  {
    fR_P_Buf = ( pR_Phi [ i-1 ] * pR_P [ i-1 ] + pR_Phi [ i ] * pR_P [ i+1 ] ) /
            ( pR_Phi [ i-1 ] + pR_Phi [ i ] );
    fR_P_Err += fabs ( fR_P_Buf - pR_P [ i ] );
    pR_P [ i ] = fR_P_Buf;
  }
  ++nN_P_Iteration;
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
  ++nN_S_Iteration;
  nN_P_Iteration = 0;
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

  nN_P_Iteration = 0;
  nN_S_Iteration = 0;


  VOID _malloc ( )
  {
    pMemPtr = _mm_malloc ( sizeof(double) * kN_GridX * 16, 8 * sizeof(double) );
    pR_S    = pMemPtr + kN_GridX * (0+0);
    pR_Phi  = pMemPtr + kN_GridX * (1+0);
    pR_f    = pMemPtr + kN_GridX * (2+0);
    pR_P    = pMemPtr + kN_GridX * (3+0);
    pR_U    = pMemPtr + kN_GridX * (4+0);

    pRG_S   = pMemPtr + kN_GridX * (0+8);
    pRG_Phi = pMemPtr + kN_GridX * (1+8);
    pRG_f   = pMemPtr + kN_GridX * (2+8);
    pRG_P   = pMemPtr + kN_GridX * (3+8);
    pRG_U   = pMemPtr + kN_GridX * (4+8);

    kR_1_dX = ((double)(kN_GridX-1));
    kR_dX = 1.0 / kR_1_dX;
    kR_dT_dX = kR_dT * kR_1_dX;
  }

  pF = fopen ( szFileName, "rb" );
  if ( pF == NULL )
  {
    pF = fopen ( szFileName, "wb" );
    rSolutionFile_HeaderWrite ( );

    _malloc ( );

    for ( UINT i = 0; i < kN_GridX; ++i )
    {
      pR_S [ i ] = rR_S_x ( ((double)i) * kR_dX );
    }
    pR_S [ 0 ] = rR_S_t ( 0.0 );

    for ( UINT i = 0; i < kN_GridX; ++i )
    {
      pR_P [ i ] = 1.0 - ( ((double)i) * kR_dX );
    }

    rSolutionFile_FrameAdd ( );
    fclose ( pF );
    pF = fopen ( szFileName, "r+b" );
  }
  else
  {
    rSolutionFile_HeaderRead ( );

    _malloc ( );

    rSolutionFile_FrameRead ( nN_S_Iteration );
  }

  rSolve_f ( );
  rSolve_Phi ( );
}

static VOID rOnDestroy ( )
{
  if ( pF )
  {
    rSolutionFile_HeaderWrite ( );
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
  if ( fR_P_Err == 0.0 )
  {
    rSolve_U ( );
    rSolve_S ( );
    rSolutionFile_FrameAdd ( );
    rSolve_f ( );
    rSolve_Phi ( );
  }
  InvalidateRect ( g_hWnd, NULL, FALSE );
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
  static UINT i = 0;
  RECT rc =
  {
    .left     = 0,
    .right    = nW,
    .top      = 0,
    .bottom   = nH/5,
  };
  if (nN_P_Iteration <= 5 )
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

  CHAR buf[512];
  TextOutA ( hDC, 0, 16*0, buf, sprintf ( buf, "Err: %.40lf", fR_P_Err ) );
  TextOutA ( hDC, 0, 16*1, buf, sprintf ( buf, "I_P: % 8u", nN_P_Iteration ) );
  TextOutA ( hDC, 0, 16*2, buf, sprintf ( buf, "I_S: % 8u", nN_S_Iteration ) );
}

LRESULT CALLBACK WndProc ( HWND hWnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam )
{
  static BOOL bMoseDown = FALSE;
  switch ( uMsg )
  {
    case WM_CREATE:
    {
      hWndDC = GetWindowDC ( hWnd );
      assert ( hWndDC );
      hBmpDC = CreateCompatibleDC ( hWndDC );
      assert ( hBmpDC );

      rOnCreate ( );
      return 0;
    }
    case WM_LBUTTONUP:
    {
      bMoseDown = FALSE;
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
      rOnPaint ( hDC );
      EndPaint ( hWnd, &ps );
      return 0;
    }
  }
  return DefWindowProc ( hWnd, uMsg, wParam, lParam );
}


INT APIENTRY wWinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance,
        LPWSTR lpCmdLine, INT nShowCmd )
{
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
