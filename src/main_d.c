#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

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

static FILE                    *pF = NULL;

// #define kType                   float
// #define kF_pow                  powf
// #define kF_fabs                 fabsf
#define kType                   long double
#define kF_pow                  powl
#define kF_fabs                 fabsl

#define kN_GridX                128
#define kN_GridT                (kN_GridX*kN_GridX*2)
#define kR_P                    ((kType)1.0e+3l)

#define kR_dX                   (((kType)1.0l)/((kType)(kN_GridX-1)))
#define kR_dT                   (((kType)1.0l)/((kType)(kN_GridT)))

// Пористость, Абсолютная проницаемость, Динамическая вязкость
// https://keldysh.ru/papers/2016/prep2016_129.pdf
#define kR_m                    ((kType)0.4l)
#define kR_K                    ((kType)6.64e-4l)
#define kR_Mw                   ((kType)1.0e-3l)
#define kR_Mn                   ((kType)1.0e-2l)

// относительные фазовые проницаемости []
// https://studopedia.ru/9_84099_obobshchenniy-zakon-darsi.html
#define kR_S0w                  ((kType)0.2l)
#define kR_S0n                  ((kType)0.15l)
#define kR_SPw                  ((kType)3.5l)
#define kR_SPn                  ((kType)2.8l)
#define kR_S_0w                 (((kType)1.0l)/(((kType)1.0l)-kR_S0w))
#define kR_S_0n                 (((kType)1.0l)/(((kType)1.0l)-kR_S0n))
#define kR_Kw(s)                ((s) < kR_S0w ? ((kType)0.0l) :\
                                kF_pow( ((s) - kR_S0w)*kR_S_0w, kR_SPw ))
#define kR_Kn(s)                ((s) < kR_S0n ? ((kType)0.0l) :\
                                kF_pow( ((s) - kR_S0n)*kR_S_0n, kR_SPn ))*\
                                (((kType)3.4l)-((kType)2.4l)*(s))
#define kR_Phi(s)               ((kR_Kw(s)) + (kR_Mw/kR_Mn)*\
                                (kR_Kn(((kType)1.0l)-s)))

/* Начальные условия */
#define kR_S0(x)                ((((kType)1.1l)-(x))*((kType)0.1l))

kType f_S   [kN_GridX];
kType f_S_  [kN_GridX];
kType f_Phi [kN_GridX];
kType f_P   [kN_GridX+1] = { [0] = kR_P, [kN_GridX] = ((kType)0.0l) };


kType f__Kw [512];
kType f__Kn [512];
kType f__Phi[512];

kType fF_S  [kN_GridX];
kType fF_S_ [kN_GridX];
kType fF_Phi[kN_GridX];
kType fF_P  [kN_GridX+1];

kType *fp_S;
kType *fp_S_;
kType *fp_Phi;
kType *fp_P;

UINT  nFramesCount = 0;
UINT  nCurrentFrame = 0;

kType f_Err;

static VOID rOnCreate ( )
{
  for ( UINT i = 0; i < 512; ++i)
  {
    f__Kw [i] = kR_Kw ((i)/((kType)(512-1)));
    f__Kn [i] = kR_Kn (((i)/((kType)(512-1))));
    f__Phi[i] = kR_Phi((i)/((kType)(512-1)));
  }

  pF = fopen ( "data.bin", "rb" );
  if ( pF )
  {
    fread ( &nFramesCount, 1, sizeof(nFramesCount), pF );
    while ( !feof ( pF ) )
    {
      fread ( f_S, 1, sizeof(f_S), pF );
      fread ( f_P, 1, sizeof(f_P), pF );
    }
    for ( UINT i = 0; i < kN_GridX; ++i )
    {
      f_Phi[i] = kR_Phi(f_S[i]);
    }
    fclose ( pF );
    pF = fopen ( "data.bin", "a+b" );
    fp_S   = f_S;
    fp_S_  = f_S_;
    fp_Phi = f_Phi;
    fp_P   = f_P;
  }
  else
  {
    f_S[0] = 1.0f;
    f_Phi[0] = kR_Phi(f_S[0]);
    for ( UINT i = 1; i < kN_GridX; ++i )
    {
      f_S[i] = kR_S0(i*kR_dX);
      f_Phi[i] = kR_Phi(f_S[i]);
    }
    pF = fopen ( "data.bin", "a+b" );
    nFramesCount = 1;
    fwrite ( &nFramesCount, 1, sizeof(nFramesCount), pF );
    fwrite ( f_S, 1, sizeof(f_S), pF );
    fwrite ( f_P, 1, sizeof(f_P), pF );
    fp_S   = f_S;
    fp_S_  = f_S_;
    fp_Phi = f_Phi;
    fp_P   = f_P;
  }
}
static VOID rOnDestroy ( )
{
  fclose ( pF );
  pF = fopen ( "data.bin", "r+b" );
  fwrite ( &nFramesCount, 1, sizeof(nFramesCount), pF );
  fclose ( pF );
}

static VOID rSolveNextStep ( )
{
  for ( UINT i = 1; i < kN_GridX; ++i )
  {
    f_S[i] += f_S_[i];
    f_Phi[i] = kR_Phi(f_S[i]);
  }
  fwrite ( f_S, 1, sizeof(f_S), pF );
  fwrite ( f_P, 1, sizeof(f_P), pF );
  ++nFramesCount;
}

static VOID rSolveStep ( )
{
  f_Err = ((kType)0.0l);
  kType fBuf;
  kType fAbs;
  for ( UINT i = 1; i < kN_GridX; ++i )
  {
    fBuf = ( (f_Phi[i]*f_P[i+1]) + (f_Phi[i-1]*f_P[i-1]) ) /
            ( f_Phi[i] + f_Phi[i-1] );
    fAbs = kF_fabs ( f_P[i] - fBuf );
    if ( f_Err < fAbs ) { f_Err = fAbs; }
    f_P[i] = fBuf;


    f_S_[i] =
            (kR_Kw(f_S[i])*(f_P[i+1]-f_P[i]))-
            (kR_Kw(f_S[i-1])*(f_P[i]-f_P[i-1]))
            * ( kR_dT/kR_dX/kR_dX )
            * ( kR_K/kR_Mw )
            ;
    //((kType)1.0l) / ( kR_Kw(f_S[i]) + kR_Kn(((kType)1.0l)-f_S[i]) );
  }
  f_S_[0] = f_Err;
  if ( f_Err < ((kType)0.00001l)*kR_P )
  {
    rSolveNextStep ( );
  }
}


static VOID rOnIdle ( )
{
  rSolveStep();
  if ( fp_P == f_P ) { InvalidateRect ( g_hWnd, NULL, FALSE ); }
}

static VOID rPlot ( const HDC hDC, const RECT rc,
        kType const * const pF, const UINT nCount,
        const HBRUSH hBr0, const HBRUSH hBr1,
        const kType fNorm_A, const kType fNorm_B )
{
  FillRect ( hDC, &rc, hBr0 );
  const UINT kW = rc.right - rc.left;
  const kType kH = rc.bottom - rc.top;
  RECT rcL = rc;
  for ( UINT ix = 0; ix < kW; )
  {
    const UINT kIx = ix * nCount / kW;
    rcL.left = ix + rc.left;
    UINT kIW = 1; ++ix;
    while ( ix * nCount / kW == kIx ) { ++kIW; ++ix; }
    rcL.right = ix + rc.left;
    const kType fY = pF[kIx] * fNorm_A + fNorm_B;
    rcL.top = rc.bottom - (fY * kH);
    FillRect ( hDC, &rcL, hBr1 );
  }
}

static VOID rOnPaint ( const HDC hDC )
{
  const UINT nH = (nWndHeight-16);
  const UINT nW = (nWndWidth/2 + nWndWidth/4);
  RECT rc =
  {
    .left     = 0,
    .right    = nW,
    .top      = 0,
    .bottom   = nH/4,
  };
  rPlot ( hBmpDC, rc,
          fp_P, kN_GridX,
          (HBRUSH)1, (HBRUSH)2,
          ((kType)1.0l)/kR_P, ((kType)0.0l) );

  rc.top      = rc.bottom;
  rc.bottom   = nH/2;
  rPlot ( hBmpDC, rc,
          fp_S_, kN_GridX,
          (HBRUSH)3, (HBRUSH)4,
          ((kType)0.5l), ((kType)0.5l) );

  rc.top      = rc.bottom;
  rc.bottom   = nH*3/4;
  rPlot ( hBmpDC, rc,
          fp_S, kN_GridX,
          (HBRUSH)1, (HBRUSH)2,
          ((kType)1.0l), ((kType)0.0l) );

  rc.top      = rc.bottom;
  rc.bottom   = nH;
  rPlot ( hBmpDC, rc,
          fp_Phi, kN_GridX,
          (HBRUSH)3, (HBRUSH)4,
          ((kType)1.0l), ((kType)0.0l) );

  rc.left     = rc.right;
  rc.right    = nWndWidth;

  rc.top      = 0;
  rc.bottom   = nH/3;
  rPlot ( hBmpDC, rc,
          f__Kw, 512,
          (HBRUSH)8, (HBRUSH)1,
          ((kType)1.0l), ((kType)0.0l) );

  rc.top      = rc.bottom;
  rc.bottom   = nH*2/3;
  rPlot ( hBmpDC, rc,
          f__Kn, 512,
          (HBRUSH)8, (HBRUSH)1,
          ((kType)1.0l), ((kType)0.0l) );

  rc.top      = rc.bottom;
  rc.bottom   = nH;
  rPlot ( hBmpDC, rc,
          f__Phi, 512,
          (HBRUSH)8, (HBRUSH)1,
          ((kType)1.0l), ((kType)0.0l) );

  rc.left     = 0;
  rc.right    = nWndWidth;
  rc.top      = rc.bottom;
  rc.bottom   = nWndHeight;

  FillRect ( hBmpDC, &rc, (HBRUSH)2 );

  if ( fp_P == f_P ) { nCurrentFrame = nFramesCount-1; }
  const UINT n_W = (nWndWidth-16);
  rc.left     = nCurrentFrame*n_W/nFramesCount;
  rc.right    = (nCurrentFrame+1)*n_W/nFramesCount+8;
  rc.top     += +4;
  rc.bottom  += -4;

  if ( fp_P == f_P ) { rc.right += 4; }

  FillRect ( hBmpDC, &rc, (HBRUSH)8 );

  CHAR buf[512];
  TextOutA ( hBmpDC, 0, 0, buf, sprintf ( buf, "Frames: %d/%d", nCurrentFrame+1, nFramesCount ) );

  BitBlt ( hDC, 0, 0, nWndWidth, nWndHeight, hBmpDC, 0, 0, SRCCOPY );
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
    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    {
      const UINT x = LOWORD(lParam);
      const UINT y = HIWORD(lParam);
      const UINT nH = (nWndHeight-16);
      const UINT n_W = (nWndWidth-16);
      if ( (y >= nH) && (uMsg == WM_LBUTTONDOWN) )
      {
        bMoseDown = TRUE;
      }
      if ( bMoseDown && (nWndHeight > 24) && (nWndWidth > 24) )
      {
        const UINT j = x*nFramesCount/n_W;
        if ( j >= nFramesCount )
        {
          fp_S   = f_S;
          fp_S_  = f_S_;
          fp_Phi = f_Phi;
          fp_P   = f_P;
        }
        else
        {
          fp_S   = fF_S;
          fp_S_  = fF_S_;
          fp_Phi = fF_Phi;
          fp_P   = fF_P;
          nCurrentFrame = j;
          fflush ( pF );
          fseek ( pF, sizeof(nFramesCount) +
                  (sizeof(fF_S)+sizeof(fF_P))*j, SEEK_SET );
          fread ( fF_S, 1, sizeof(fF_S), pF );
          fread ( fF_P, 1, sizeof(fF_P), pF );

          fF_Phi[0] = kR_Phi(fF_S[0]);
          for ( UINT i = 1; i < kN_GridX; ++i )
          {
            fF_Phi[i] = kR_Phi(fF_S[i]);
            fF_S_[i] =
                    (kR_Kw(fF_S[i])*(fF_P[i+1]-fF_P[i]))-
                    (kR_Kw(fF_S[i-1])*(fF_P[i]-fF_P[i-1]))
                    * ( kR_dT/kR_dX/kR_dX )
                    * ( kR_K/kR_Mw )
                    ;
          }
        }
        InvalidateRect ( hWnd, NULL, FALSE );
      }
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

#include <intrin.h>

  volatile __m128d p[4];
  volatile double dd[16] = { };

INT APIENTRY wWinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance,
        LPWSTR lpCmdLine, INT nShowCmd )
{

  dd[1] = 1.0;
  dd[2] = 2.0;
  dd[3] = 3.0;
  dd[4] = 4.0;
  dd[5] = 5.0;
  dd[6] = 6.0;

  p[0] = _mm_loadu_pd(dd);
  p[1] = _mm_load_pd(dd);
  p[2] = _mm_loadu_pd((double*)(((PBYTE)dd)+3));
  p[3] = _mm_load_pd((double*)(((PBYTE)dd)+3));

  _mm_store_pd(dd,_mm_add_pd(p[0],p[2]));
  _mm_store_pd(dd+3,_mm_add_pd(p[1],p[3]));

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
