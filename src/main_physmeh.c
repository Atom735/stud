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

// Контекст одномерной задачи переноса
struct ctx_s1d
{
  // Количество узлов в сетке
  UINT32                        kGridX;
  UINT32                        kGridT;
  BOOL                          kLimitation;
  // Количество расчитываемых слоёв времени
  // 0 - если расчёт идёт до бесконечности
  UINT32                        kNTimes;
  // Количество расчитаных слоёв времени
  UINT32                        nNTImes;
  // Количество слоёв времени хранимых в памяти
  UINT32                        kMemNTimes;
  // Название файла для записи данных
  // NULL - если не требуется запись
  LPSTR                         szFileName;
  // Указатель на функцию решатель
  VOID                          (*rSolver)( struct ctx_s1d * const );
  // Указатель на функицю рисователь
  // NULL - если не требуется рисовать
  VOID                          (*rPainter)( struct ctx_s1d * const );
  // Указатель на функцию начальных и граничных условий
  FLOAT                         (*rU)( const FLOAT );
  // Константы для решалки
  FLOAT                         kF_dT;
  FLOAT                         kF_dX;

  FLOAT                         fErr;

  PFLOAT                       *pU;
};

// Функция заданая квадратной волной
static FLOAT rU_WaveQuad ( const FLOAT x )
{
  return cosf ( x * ((FLOAT)M_PI) * 4.0f ) > 0.0f ? 1.0f : 0.0f;
}
// Функция заполнения пилой
static FLOAT rU_WaveSaw ( const FLOAT x )
{
  if ( x > 0.0f ) return fmodf ( x * 4.0f, 1.0f );
  return 1.0f - fmodf ( -x * 4.0f, 1.0f );
}
// Функция заполнения пилой
static FLOAT rU_WaveSawR ( const FLOAT x )
{
  if ( x > 0.0f ) return 1.0f - fmodf ( x * 4.0f, 1.0f );
  return fmodf ( -x * 4.0f, 1.0f );
}
// Функция заполнения инусоидой
static FLOAT rU_WaveSin ( const FLOAT x )
{
  return ( sinf ( x * ((FLOAT)M_PI) * 4.0f ) + 1.0f ) * 0.5f;
}

// Функция рисолвальщик
static VOID rS1D_Painter ( struct ctx_s1d * const p )
{
  const PUINT32 pB = ((PUINT32)pBmpBuf);
  const UINT kH = nWndHeight;
  const UINT kX = p -> kGridX;
  const PFLOAT pU1 = p -> pU [ 0 ];
  const FLOAT kFT = p -> nNTImes * p -> kF_dT;
  for ( UINT ix = 0; ix < nWndWidth; ++ix )
  {
    const UINT kIx = ix * kX / nWndWidth;
    const FLOAT fY1 = 0.25f + 0.5f * pU1 [ kIx ];
    const FLOAT fY2 = 0.25f + 0.5f * p -> rU ( kIx * p -> kF_dX - kFT );
    const UINT kIy1 = kH - ((UINT)( fY1 * kH ));
    const UINT kIy2 = kH - ((UINT)( fY2 * kH ));
    for ( UINT iy = 0; iy < kH; ++iy )
    {
      pB [ iy * nBmpWidth + ix ] =
        ( iy < kIy1 ) ? 0x00ffffff^0x001f3fff:0x004f3f2f;
      pB [ iy * nBmpWidth + ix ] ^=
        ( iy < kIy2 ) ? 0x00ffffff:0;
    }
  }
}

// Решатель по  Схеме "Чехарда"
static VOID  rS1D_Solver_1 ( struct ctx_s1d * const p )
{
  // Смещаем буффера
  {
    const PFLOAT _pUBuf = p -> pU [ p -> kMemNTimes - 1 ];
    for ( UINT i = p -> kMemNTimes - 1; i != 0; --i )
    {
      p -> pU [ i ] = p -> pU [ i-1 ];
    }
    p -> pU [ 0 ] = _pUBuf;
  }
  // Увеличиваем количество итераций
  ++ p -> nNTImes;
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

  const PFLOAT pU1 = p -> pU [ 0 ];
  const PFLOAT pU2 = p -> pU [ 1 ];
  const PFLOAT pU3 = p -> pU [ 2 ];


  pU1 [ 0 ] =  p -> rU ( -((FLOAT)( p -> nNTImes )) * p -> kF_dT );
  const FLOAT kF_dT_dX = p -> kF_dT / p -> kF_dX;

  for ( UINT i = 1; i < p -> kGridX - 1; ++i )
  {
    pU1 [ i ] = pU3 [ i ] +
        ( ( pU2 [ i-1 ] - pU2 [ i+1 ] ) * kF_dT_dX );
  }
  if ( p -> kLimitation )
  {
    for ( UINT i = 1; i < p -> kGridX - 1; ++i )
    {
      if ( pU1 [ i ] > 1.0f ) { pU1 [ i ] = 1.0f; }
      if ( pU1 [ i ] < 0.0f ) { pU1 [ i ] = 0.0f; }
    }
  }
  pU1 [ p -> kGridX - 1 ] = pU1 [ p -> kGridX - 2 ];
}

// Решатель по неявной четырехточечной схеме
static VOID  rS1D_Solver_2 ( struct ctx_s1d * const p )
{
  // Смещаем буффера
  if ( p -> fErr < 0.0001f )
  {
    const PFLOAT _pUBuf = p -> pU [ p -> kMemNTimes - 1 ];
    for ( UINT i = p -> kMemNTimes - 1; i != 0; --i )
    {
      p -> pU [ i ] = p -> pU [ i-1 ];
    }
    p -> pU [ 0 ] = _pUBuf;
    // Увеличиваем количество итераций
    ++ p -> nNTImes;
  }
  /*
    Неявная четырехточечная схема
    u[+,-] == u[+,0] == u[+,+]
                ||
              u[0,0]

    u[+,0] - u[0,0]     u[+,+] - u[+,-]
    ---------------  +  --------------- = 0
           dt                 2dx



    2dx u[+,0] + dt u[+,+] - dt u[+,-] = 2dx u[0,0]
    // Метод простых итераций
    u[+,0] = u[0,0] + ( u[+,-] - u[+,+] ) dt / 2dx

  */

  const PFLOAT pU1 = p -> pU [ 0 ];
  const PFLOAT pU2 = p -> pU [ 1 ];

  pU1 [ 0 ] =  p -> rU ( -((FLOAT)( p -> nNTImes )) * p -> kF_dT );
  const FLOAT kF_dT_2dX = p -> kF_dT * 0.5f / p -> kF_dX;
  p -> fErr = 0.0f;
  for ( UINT i = 1; i < p -> kGridX - 1; ++i )
  {
    const FLOAT f = pU2 [ i ] +
        ( ( pU1 [ i-1 ] - pU1 [ i+1 ] ) * kF_dT_2dX );
    p -> fErr += fabsf ( f - pU1 [ i ] );
    pU1 [ i ] = f;
  }
  pU1 [ p -> kGridX - 1 ] = pU1 [ p -> kGridX - 2 ];
  // if ( p -> kLimitation )
  // {
  //   for ( UINT i = 1; i < p -> kGridX - 1; ++i )
  //   {
  //     if ( pU1 [ i ] > 1.0f ) { pU1 [ i ] = 1.0f; }
  //     if ( pU1 [ i ] < 0.0f ) { pU1 [ i ] = 0.0f; }
  //   }
  // }
}

static struct ctx_s1d * rS1D_Create_1 ( const UINT kGridX, const UINT kGridT,
        const UINT kNTimes, const LPCSTR szFileName )
{

  const UINT kMemNTimes = 2;
  const UINT nSFN = szFileName ? strlen ( szFileName ) + 1 : 0;
  const UINT nSNU = sizeof(PFLOAT) * kMemNTimes;
  const UINT nSS = sizeof(struct ctx_s1d) + sizeof(FLOAT) * kGridX * kMemNTimes;
  const UINT nSSF = nSNU + nSS + nSFN;
  struct ctx_s1d * const p = (struct ctx_s1d*) malloc ( nSSF );
  assert ( p );
  memset ( p, 0, nSSF );
  p -> kGridX                   = kGridX;
  p -> kGridT                   = kGridT;
  p -> kLimitation              = FALSE;
  p -> kNTimes                  = kNTimes;
  p -> kMemNTimes               = kMemNTimes;
  p -> rSolver                  = rS1D_Solver_2;
  p -> rPainter                 = rS1D_Painter;
  p -> rU                       = rU_WaveSawR;
  p -> kF_dX                    = 1.0f / (FLOAT)(kGridX-1);
  p -> kF_dT                    = 1.0f / (FLOAT)(kGridT-1);
  p -> pU = (PFLOAT*)( ((PVOID)p) + sizeof(struct ctx_s1d) );
  p -> pU [ 0 ] = (PFLOAT)( ((PVOID)p) + sizeof(struct ctx_s1d) + nSNU );
  for ( UINT i = 1; i < kMemNTimes; ++i )
  {
    p -> pU [ i ] = p -> pU [ i-1 ] + kGridX;
  }
  if ( szFileName )
  {
    p -> szFileName = (LPSTR)( ((PVOID)p) + nSS + nSNU );
    memcpy ( p -> szFileName, szFileName, nSFN );
  }

  for ( UINT ix = 0; ix < kGridX; ++ix )
  {
    const FLOAT fU = p -> rU ( ix * p -> kF_dX );
    for ( UINT i = 0; i < kMemNTimes; ++i )
    {
      (p -> pU [ i ])[ ix ] = fU;
    }
  }

  return p;
}

static struct ctx_s1d * pS1D = NULL;

static VOID rOnCreate ( )
{
  pS1D = rS1D_Create_1 ( 1024, 2048, 0, NULL );
  assert ( pS1D );
}


static VOID rOnIdle ( )
{
  assert ( pS1D );
  pS1D -> rSolver ( pS1D );
  InvalidateRect ( g_hWnd, NULL, FALSE );
}

static VOID rOnPaint ( const HDC hDC )
{
  assert ( pS1D );
  pS1D -> rPainter ( pS1D );

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



