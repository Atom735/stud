#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <windows.h>

#include <GL/glcorearb.h>
#include <GL/wglext.h>

static HINSTANCE                g_hInstance                 = NULL;
static HWND                     g_hWnd                      = NULL;
static HDC                      g_hWndDC                    = NULL;
static HGLRC                    g_hGLRC                     = NULL;
static HMODULE                  g_hModule_OpenGL            = NULL;
static const LPCTSTR            g_cszClassName = L"WCT: Gilyazeev A.R.";

static GLuint                   g_iVAO;
static GLuint                   g_iVBO;
static GLuint                   g_iShaderProgram;

#define D_INC_GL(ptr,name)      static ptr name = NULL;
#include "inc_gl.x"
#undef D_INC_GL
static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;


static LPVOID rFileToBuf ( const LPCSTR szFileName )
{
  FILE * const pF = fopen ( szFileName, "rb" );
  if ( pF == NULL ) return NULL;
  fseek ( pF, 0, SEEK_END );
  const UINT nFileSize = ftell ( pF );
  const LPVOID pBuf = malloc ( nFileSize+1 );
  fseek ( pF, 0, SEEK_SET );
  fread ( pBuf, 1, nFileSize, pF );
  fclose ( pF );
  ((PBYTE)pBuf)[nFileSize] = 0x00;
  return pBuf;
}

#define D_GL_LoadFunc(name)     (name = rGL_GetFuncAddress ( #name ))
static LPVOID rGL_GetFuncAddress ( const LPCSTR szFuncName )
{
  const LPVOID p = (LPVOID) wglGetProcAddress ( szFuncName );
  return p == NULL ? GetProcAddress ( g_hModule_OpenGL, szFuncName ) : p;
}
static GLuint rGL_LoadShader ( const LPCSTR szFileName, const GLenum iType )
{
  const LPVOID pSource = rFileToBuf ( szFileName );
  const GLuint iShader = glCreateShader ( iType );
  glShaderSource ( iShader, 1, (const GLchar**)&pSource, 0 );
  glCompileShader ( iShader );
  GLint iInt;
  glGetShaderiv ( iShader, GL_COMPILE_STATUS, &iInt );
  if ( iInt == FALSE )
  {
    glGetShaderiv ( iShader, GL_INFO_LOG_LENGTH, &iInt );
    const LPSTR szLog = malloc ( iInt );
    glGetShaderInfoLog ( iShader, iInt, &iInt, szLog );
    printf ( szLog );
    free ( szLog );
  }
  free ( pSource );
  return iShader;
}

static LRESULT rOnCreate ( )
{
  PIXELFORMATDESCRIPTOR pfd =
  {
    .nSize          = sizeof ( PIXELFORMATDESCRIPTOR ),
    .nVersion       = 1,
    .dwFlags        =
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    .iPixelType     = PFD_TYPE_RGBA,
    .cColorBits     = 32,
    .cRedBits       = 0,
    .cRedShift      = 0,
    .cGreenBits     = 0,
    .cGreenShift    = 0,
    .cBlueBits      = 0,
    .cBlueShift     = 0,
    .cAlphaBits     = 0,
    .cAlphaShift    = 0,
    .cAccumBits     = 0,
    .cAccumRedBits  = 0,
    .cAccumGreenBits= 0,
    .cAccumBlueBits = 0,
    .cAccumAlphaBits= 0,
    .cDepthBits     = 24,
    .cStencilBits   = 0,
    .cAuxBuffers    = 0,
    .iLayerType     = PFD_MAIN_PLANE,
    .bReserved      = 0,
    .dwLayerMask    = 0,
    .dwVisibleMask  = 0,
    .dwDamageMask   = 0,
  };
  g_hWndDC = GetDC ( g_hWnd );
  assert ( g_hWndDC );

  INT iPixForm =  ChoosePixelFormat ( g_hWndDC, &pfd );
  assert ( iPixForm );
  SetPixelFormat ( g_hWndDC, iPixForm, &pfd );
  const HGLRC hGLRC_temp = wglCreateContext ( g_hWndDC );
  assert ( hGLRC_temp );
  wglMakeCurrent ( g_hWndDC, hGLRC_temp );
  D_GL_LoadFunc ( wglCreateContextAttribsARB );
  assert ( wglCreateContextAttribsARB );
  wglMakeCurrent ( NULL, NULL );
  wglDeleteContext ( hGLRC_temp );
  const INT iAttr[] =
  {
    WGL_CONTEXT_MAJOR_VERSION_ARB,      3,
    WGL_CONTEXT_MINOR_VERSION_ARB,      3,
    WGL_CONTEXT_FLAGS_ARB,              WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
    WGL_CONTEXT_PROFILE_MASK_ARB,       WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
    0
  };
  g_hGLRC = wglCreateContextAttribsARB ( g_hWndDC, NULL, iAttr );
  assert ( g_hGLRC );
  wglMakeCurrent ( g_hWndDC, g_hGLRC );
  #define D_INC_GL(ptr,name) D_GL_LoadFunc(name);
  #include "inc_gl.x"
  #undef D_INC_GL

  const GLuint iShaderVertex = rGL_LoadShader ( "glsl/v.x", GL_VERTEX_SHADER );
  const GLuint iShaderFragment = rGL_LoadShader ( "glsl/f.x", GL_FRAGMENT_SHADER );
  g_iShaderProgram =  glCreateProgram ( );
  glAttachShader ( g_iShaderProgram, iShaderVertex );
  glAttachShader ( g_iShaderProgram, iShaderFragment );
  glBindAttribLocation ( g_iShaderProgram, 0, "in_Position" );
  glLinkProgram ( g_iShaderProgram );
  GLint iInt;
  glGetProgramiv ( g_iShaderProgram, GL_LINK_STATUS, &iInt);
  if ( iInt == FALSE )
  {
    glGetProgramiv ( g_iShaderProgram, GL_INFO_LOG_LENGTH, &iInt );
    const LPSTR szLog = malloc ( iInt );
    glGetProgramInfoLog ( g_iShaderProgram, iInt, &iInt, szLog );
    printf ( szLog );
    free ( szLog );
  }
  glUseProgram ( g_iShaderProgram );

  GLfloat f[512+4];
  for ( UINT i=0; i<=128; ++i )
  {
    const FLOAT _f = (((GLfloat)(i)))/(GLfloat)(128);

    f[i*4+0] = sinf ( _f * M_PI * 2 )*0.05f;
    f[i*4+1] = cosf ( _f * M_PI * 2 )*0.05f;
    f[i*4+2] = sinf ( _f * M_PI * 2 )*0.95f;
    f[i*4+3] = cosf ( _f * M_PI * 2 )*0.95f;
  }
  glGenVertexArrays ( 1, &g_iVAO );
  glBindVertexArray ( g_iVAO );
  glGenBuffers ( 1, &g_iVBO );
  glBindBuffer ( GL_ARRAY_BUFFER, g_iVBO );
  glBufferData ( GL_ARRAY_BUFFER, (512+4) * sizeof(GLfloat), f, GL_STATIC_DRAW );
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
  glEnableVertexAttribArray ( 0 );
  return 0;
}

static LRESULT rOnDestroy ( )
{
  glUseProgram ( 0 );
  glDisableVertexAttribArray ( 0 );
  glDeleteProgram ( g_iShaderProgram );
  glDeleteBuffers ( 1, &g_iVBO );
  glDeleteVertexArrays ( 1, &g_iVAO );
  wglMakeCurrent ( NULL, NULL );
  wglDeleteContext ( g_hGLRC );
  ReleaseDC ( g_hWnd, g_hWndDC );
  PostQuitMessage ( 0 );
  return 0;
}
static LRESULT rOnSize ( UINT nWidth, UINT nHeight )
{
  glViewport ( 0, 0, nWidth, nHeight );
  return 0;
}

static VOID rOnIdle ( )
{
  glClearColor ( 0.0, 0.0, 0.0, 1.0 );
  glClear ( GL_COLOR_BUFFER_BIT );
  glDrawArrays ( GL_TRIANGLE_STRIP, 0, 256+2 );
  SwapBuffers ( g_hWndDC );
}


static LRESULT CALLBACK rWndProc ( HWND hWnd, UINT iMsg, WPARAM wParam,
        LPARAM lParam )
{
  switch ( iMsg )
  {
    case WM_CREATE:
      g_hWnd = hWnd;
      return rOnCreate ( );
    case WM_DESTROY:
      return rOnDestroy ( );
    case WM_SIZE:
      return rOnSize ( LOWORD(lParam), HIWORD(lParam) );
  }
  return DefWindowProc ( hWnd, iMsg, wParam, lParam );
}

INT APIENTRY wWinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance,
        LPWSTR lpCmdLine, INT nShowCmd )
{
  g_hInstance = hInstance;
  g_hModule_OpenGL = LoadLibraryA ( "opengl32.dll" );
  assert ( g_hModule_OpenGL );
  {
    WNDCLASSEX wc = {
      .cbSize           = sizeof ( WNDCLASSEXW ),
      .style            = CS_OWNDC,
      .lpfnWndProc      = rWndProc,
      .cbClsExtra       = 0,
      .cbWndExtra       = 0,
      .hInstance        = g_hInstance,
      .hIcon            = NULL,
      .hCursor          = NULL,
      .hbrBackground    = NULL,
      .lpszMenuName     = NULL,
      .lpszClassName    = g_cszClassName,
      .hIconSm          = NULL,
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
    while ( PeekMessage ( &msg, NULL, 0, 0, PM_REMOVE ) )
    {
      if ( msg.message == WM_QUIT ) { goto P_End; }
      TranslateMessage ( &msg );
      DispatchMessage ( &msg );
    }
    rOnIdle ( );
  }
  P_End:
  UnregisterClass ( g_cszClassName, g_hInstance );
  FreeLibrary ( g_hModule_OpenGL );
  return msg.wParam;
}

