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

#define kGridX                  4096*16

static FLOAT                    _U1 [ kGridX ];
static FLOAT                    _U2 [ kGridX ];
static FLOAT                    _U3 [ kGridX ];
static FLOAT                   *_pU1;
static FLOAT                   *_pU2;
static FLOAT                   *_pU3;

static const FLOAT              _d_t    = 0.5f/((FLOAT)(kGridX-1));
static const FLOAT              _d_x    = 1.0f/((FLOAT)(kGridX-1));

static UINT nI = 0;

// Начальные значения
static FLOAT rU_Init ( const FLOAT x )
{
  const FLOAT c = cosf(x*10.f);
  return x < 0.0f ? 0.0f :
    // x < 0.15f ? 1.0f : 0.0f;
    c > 0.0f ? c : 0.0f;
}
// Граничные значение
static FLOAT rU_Left ( const FLOAT t )
{
  const FLOAT c = powf(cosf(t*17.0f),0.1f);
  return t < 0.0f ? 0.0f :
    // t < 0.1f ? 1.0f : 0.0f;
    c > 0.0f ? c : 0.0f;
}

static FLOAT rU_Original ( const FLOAT t, const FLOAT x )
{
  return rU_Init ( x-t ) + rU_Left ( t-x );
}

static VOID rOnCreate ( )
{
  _pU1 = _U1;
  _pU2 = _U2;
  _pU3 = _U3;

  for ( UINT i = 0; i < kGridX; ++i )
  {
    _pU3 [ i ] = _pU2 [ i ] = _pU1 [ i ] =
            rU_Init ( ((FLOAT)i) * _d_x );

  }
}

static VOID rOnIdle ( )
{
  /*
    Схема "Чехарда"
              u[+,0]
                ||
    u[0,-] == u[0,0] == u[0,+]
                ||
              u[-,0]

    u[+,0] - u[-,0]     u[0,+] - u[0,-]
    ---------------  +  --------------- = 0
          2dt                 2dx


                       u[0,-] - u[0,+]
    u[+,0]  = u[-,0] + --------------- 2dt
                             2dx

  */
  ++nI;

    _pU1 [ 0 ] =  rU_Left ( ((FLOAT)nI) * _d_t );

  for ( UINT i = 1; i < kGridX - 1; ++i )
  {
    _pU1 [ i ] = _pU3 [ i ] +
        ( ( _pU2 [ i-1 ] - _pU2 [ i+1 ] ) * _d_t / _d_x );

    // if ( _pU [ i ] > 1.0f ) { _pU [ i ] = 0.0f; }
    // if ( _pU [ i ] < 0.0f ) { _pU [ i ] = 0.0f; }
  }
    _pU1 [ kGridX - 1 ] = _pU1 [ kGridX - 2 ];

  {
    const PFLOAT _pUBuf = _pU1;
    _pU1 = _pU3;
    _pU3 = _pU2;
    _pU2 = _pUBuf;
  }

  InvalidateRect ( g_hWnd, NULL, FALSE );
}

static VOID rOnPaint ( const HDC hDC )
{
  const PUINT32 pB = ((PUINT32)pBmpBuf);
  const UINT nH = nWndHeight;
  for ( UINT ix = 0; ix < nWndWidth; ++ix )
  {
    const UINT kx = ix * kGridX / nWndWidth;
    #define _D1 ( _pU2 [ kx ] )
    #define _D2 ( rU_Original ( nI * _d_t, kx * _d_x ) )
    const UINT k_1 = nH - ((UINT)(( _D1 * 0.5f + 0.25f ) * nH));
    const UINT k_2 = nH - ((UINT)(( _D2 * 0.5f + 0.25f ) * nH));
    for ( UINT iy = 0; iy < nH; ++iy )
    {
      pB [ iy * nBmpWidth + ix ] =
        ( iy < k_1 ) ? 0x00ffffff^0x001f3fff:0x004f3f2f;
      pB [ iy * nBmpWidth + ix ] ^=
        ( iy < k_2 ) ? 0x00ffffff:0;
    }
  }

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

      rOnCreate ( );

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
      rOnPaint ( hDC );
      BitBlt ( hDC, 0, 0, nWndWidth, nWndHeight, hBmpDC, 0, 0, SRCCOPY );
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
  g_hWnd = CreateWindowEx ( 0, g_cszClassName, NULL, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
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
  P_END:

  return msg.wParam;
}



