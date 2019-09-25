#include <assert.h>
#include <math.h>

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

#define kGridX                  4096

// Заводнёность в узлах
static FLOAT                    g_s1 [ kGridX ];
static FLOAT                    g_s2 [ kGridX ];
static PFLOAT                   g_s = g_s1;
static PFLOAT                   g_s_ = g_s2;
// Скорость первого флюида в полуузлах, начиная с (1/2) между 0 и 1
static FLOAT                    g_uh [ kGridX ];
// Скорость первого флюида в узлах
static FLOAT                    g_u [ kGridX ];
// Давление в полуузлах, начиная с (1/2) между 0 и 1
static FLOAT                    g_ph [ kGridX ];
// Давление в узлах
static FLOAT                    g_p [ kGridX ];

// Динамическая вязкость:
// http://ru.wikipedia.org/wiki/Вязкость#Динамическая_вязкость_разных_веществ
static const FLOAT              mu_1 = 0.894f;
static const FLOAT              mu_2 = 9.85f;

static FLOAT rF_1 ( const FLOAT s )
{
  return powf ( s, 2.0f );
}
static FLOAT rF_2 ( const FLOAT s )
{
  return powf ( 1.0f-s, 2.7f );
}

LRESULT CALLBACK WndProc ( HWND hWnd, UINT uMsg, WPARAM wParam,
        LPARAM lParam )
{

  switch ( uMsg )
  {
    case WM_CREATE:
    {
      hWndDC = GetWindowDC ( hWnd );
      assert ( hWndDC );
      hBmpDC = CreateCompatibleDC ( hWndDC );
      assert ( hBmpDC );

      g_s2 [ 0 ] = g_s1 [ 0 ] = 0.9f;
      for ( UINT i = 1; i < kGridX; ++i )
      {
        g_s2 [ i ] = g_s1 [ i ] = 0.4f-i*0.2f/kGridX;
      }

      return 0;
    }
    case WM_SIZE: {
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
      BitBlt ( hDC, 0, 0, nWndWidth, nWndHeight, hBmpDC, 0, 0, SRCCOPY );
      EndPaint ( hWnd, &ps );
      return 0;
    }
  }
  return DefWindowProc ( hWnd, uMsg, wParam, lParam );
}

static VOID rIdle ( )
{
  // Расчёт давления в полуузлах
  g_ph [ 0 ] = 0.0f;
  for ( UINT i = 1; i < kGridX; ++i )
  {
    // Перепад давления в узле
    const FLOAT d_p = mu_1 * mu_2 /
            ( rF_1 ( g_s [ i ] ) * mu_2 + rF_2 ( g_s [ i ] ) * mu_1 );
    g_ph [ i ] = g_ph [ i-1 ] + d_p;
  }
  // Расчёт давлений в узлах
  g_p [ 0 ] = 0.0f;
  for ( UINT i = 1; i < kGridX; ++i )
  {
    // Перепад давления в узле
    g_p [ i ] = ( g_ph [ i-1 ] + g_ph [ i ] ) * 0.5f;
  }
  // Расчёт скоростей в полуузлах
  for ( UINT i = 0; i < kGridX-1; ++i )
  {
    // Перепад давления в полуузле
    const FLOAT d_p = ( g_p [ i+1 ] - g_p [ i ] ) / mu_1;
    g_uh [ i ] = d_p * rF_1 ( (g_s [ i ] + g_s [ i+1 ]) * 0.5f );
  }
  g_uh [ kGridX-1 ] = 1.0f;
  // Расчёт новго значения заводнения в узлах
  for ( UINT i = 1; i < kGridX-1; ++i )
  {
    // Перепад давления в полуузле
    const FLOAT d_s =
            g_uh [ i ] * rF_1 ( (g_s [ i ] + g_s [ i+1 ]) * 0.5f ) -
            g_uh [ i-1 ] * rF_1 ( (g_s [ i ] + g_s [ i-1 ]) * 0.5f );
    g_s_ [ i ] = g_s [ i ] + d_s * 0.01f;
  }


  const PUINT32 pB = ((PUINT32)pBmpBuf);
  const UINT nH = nWndHeight >> 2;
  for ( UINT ix = 0; ix < nWndWidth; ++ix )
  {
    const UINT kx = ix * kGridX / nWndWidth;
    const UINT k_1 = ( g_s [ kx ] ) * nH;
    for ( UINT iy = 0; iy < nH; ++iy )
    {
      pB [ iy * nBmpWidth + ix ] =
        ( iy < k_1 ) ? 0x001f3fff:0x004f3f2f;
    }
    const UINT k_2 = ( g_p [ kx ] / g_p [ kGridX-1 ] ) * nH + nH;
    for ( UINT iy = nH; iy < nH*2; ++iy )
    {
      pB [ iy * nBmpWidth + ix ] =
        ( iy > k_2 ) ? 0x001f7faf:0x001f1f1f;
    }
    const UINT k_3 = ( g_uh [ kx ] ) * nH + nH*2;
    for ( UINT iy = nH*2; iy < nH*3; ++iy )
    {
      pB [ iy * nBmpWidth + ix ] =
        ( iy < k_3 ) ? 0x003f4fff:0x007f6f4f;
    }
  }

  if ( g_s == g_s1 )
  {
    g_s   = g_s2;
    g_s_  = g_s1;
  }
  else
  {
    g_s   = g_s1;
    g_s_  = g_s2;
  }

  InvalidateRect ( g_hWnd, NULL, FALSE );
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
  g_hWnd = CreateWindowEx ( 0, g_cszClassName, NULL, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
          NULL, NULL, g_hInstance, NULL );

  BOOL bRun = TRUE;
  MSG msg;
  while ( bRun )
  {
    while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
    {
      if ( msg.message == WM_QUIT ) { bRun = FALSE; goto P_END; }
      TranslateMessage ( &msg );
      DispatchMessage ( &msg );
    }
    rIdle ( );
  }
  P_END:

  return msg.wParam;
}



