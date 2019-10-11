#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>

#include <windows.h>
#include <shellapi.h>

#include <GL/glcorearb.h>
#include <GL/wglext.h>

/*
main -G -S_x 0.1 -txt data/d1.txt data/d1.bin -G -S_x 0.2 -txt data/d2.txt data/d2.bin
*/

static HINSTANCE                g_hInstance                 = NULL;
static HWND                     g_hWnd                      = NULL;
static HDC                      g_hWndDC                    = NULL;
static UINT                     g_nWndWidth                 = 0;
static UINT                     g_nWndHeight                = 0;
static HGLRC                    g_hGLRC                     = NULL;
static HMODULE                  g_hModule_OpenGL            = NULL;
static const LPCTSTR            g_cszClassName = L"WCT: Gilyazeev A.R.";
static LPSTR                   *g_szArgList                 = NULL;
static INT                      g_nArgs;

#define D_MAGIC_V_1             UINT64_C(0xE75FB92DEA5243CD)

struct solution_head
{
  UINT64                        kI_Magic;
  UINT32                        kN_FrameCount;
  UINT32                        kN_GridX;
  UINT32                        kN_ItersIdentMin;
  UINT32                        kN_ItersSkips;
  double                        kR_ErrorMax;
  double                        kR_Alpha;
  double                        kR_K_Mu;
  double                        kR_dT;
  double                        kR_S_t;
  double                        kR_S_x;
}; // sizeof() == 72

struct solution
{
  struct solution              *pPrev;
  struct solution              *pNext;
  struct solution_head          head;
  CHAR                          szFileName[256];
  LPCSTR                        szFileNameTxt;
  FILE                         *pF;
  double                        kR_dX;
  double                        kR_1_dX;
  double                        kR_dT_dX;
  UINT                          kN_FrameCountMax;

  double                       *pMemPtr;
  double                       *pR_S;
  double                       *pR_f;
  double                       *pR_Phi;
  double                       *pR_P;
  double                       *pR_U;
  double                       *pR_1_Sum_Phi;
  double                        fR_P_Err;

  UINT                          nPII;
  UINT                          nPIS;
  BOOL                          bReWrite;
  BOOL                          bSolved;
  UINT                          iPlotN;
  BOOL                          bDirty;
};

static struct solution         *pS_First                    = NULL;
static struct solution         *pS_Last                     = NULL;
static UINT                     kN_SolCount                 = 0;
static UINT                     kN_SolCountPlot             = 0;

static struct solution * rSolution_New ( )
{
  struct solution * const p = (struct solution*)
          malloc ( sizeof ( struct solution ) );
  memset ( p, 0, sizeof ( struct solution ) );
  p -> head. kI_Magic           = D_MAGIC_V_1;
  p -> head. kN_GridX           = 128;
  p -> head. kN_ItersIdentMin   = 8;
  p -> head. kN_ItersSkips      = 0;
  p -> head. kR_Alpha           = 2.0;
  p -> head. kR_K_Mu            = 1e-1;
  p -> head. kR_dT              = 1e-01;
  p -> head. kR_S_t             = 1.0;
  p -> head. kR_S_x             = 0.0;
  return p;
}

static void rSolution_DbgHeadReWrite ( struct solution * const p )
{
  if ( p -> szFileNameTxt )
  {
    FILE * const pF = fopen ( p -> szFileNameTxt, "wb" );
    #define D_print(...) fprintf ( pF, __VA_ARGS__ )
    D_print ( "kI_Magic             = %016" PRIx64 "\n", p -> head. kI_Magic );
    D_print ( "kN_FrameCount        = %" PRIu32 "\n", p -> head. kN_FrameCount );
    D_print ( "kN_GridX             = %" PRIu32 "\n", p -> head. kN_GridX );
    D_print ( "kN_ItersIdentMin     = %" PRIu32 "\n", p -> head. kN_ItersIdentMin );
    D_print ( "kN_ItersSkips        = %" PRIu32 "\n", p -> head. kN_ItersSkips );
    D_print ( "kR_ErrorMax          = %.20f\n", p -> head. kR_ErrorMax );
    D_print ( "kR_Alpha             = %.20f\n", p -> head. kR_Alpha );
    D_print ( "kR_K_Mu              = %.20f\n", p -> head. kR_K_Mu );
    D_print ( "kR_dT                = %.20f\n", p -> head. kR_dT );
    D_print ( "kR_S_t               = %.20f\n", p -> head. kR_S_t );
    D_print ( "kR_S_x               = %.20f\n", p -> head. kR_S_x );
    D_print ( "======================\n" );
    D_print ( "szFileName           = %s\n", p -> szFileName );
    D_print ( "kR_dX                = %.20f\n", p -> kR_dX );
    D_print ( "kR_1_dX              = %.20f\n", p -> kR_1_dX );
    D_print ( "kR_dT_dX             = %.20f\n", p -> kR_dT_dX );
    D_print ( "kN_FrameCountMax     = %d\n", p -> kN_FrameCountMax );
    D_print ( "bReWrite             = %s\n", p -> bReWrite ? "TRUE" : "FALSE" );
    D_print ( "iPlotN               = %d\n", p -> iPlotN );
    D_print ( "======================\n" );
    #undef D_print
    fclose ( pF );
  }
}

static void rSolution_Init ( struct solution * const p )
{
  const UINT kN_GridX = p -> head. kN_GridX;
  const UINT kN_FrameSz = sizeof(double) * kN_GridX;
  void _rMalloc ( )
  {
    double * const pMemPtr = (double*) _mm_malloc (
              kN_FrameSz * 6, 8 * sizeof(double) );

    p -> pMemPtr                = pMemPtr;
    p -> pR_S                   = pMemPtr + kN_GridX * 0;
    p -> pR_f                   = pMemPtr + kN_GridX * 1;
    p -> pR_Phi                 = pMemPtr + kN_GridX * 2;
    p -> pR_P                   = pMemPtr + kN_GridX * 3;
    p -> pR_U                   = pMemPtr + kN_GridX * 4;
    p -> pR_1_Sum_Phi           = pMemPtr + kN_GridX * 5;

    p -> kR_dX = 1.0 / ( p -> kR_1_dX = ((double)(kN_GridX-1)) );
    p -> kR_dT_dX = p -> head. kR_dT * p -> kR_1_dX;

    p -> pR_P [ 0 ] = 1.0;
    p -> pR_P [ kN_GridX-1 ] = 0.0;
    for ( UINT i = 1; i < kN_GridX-1; ++i )
    {
      p -> pR_P [ i ] = 1.0 - ( ((double)i) * ( p -> kR_dX ) );
    }
  }

  if ( p -> bReWrite )
  {
    P_newfile:
    p -> pF = fopen ( p -> szFileName, "wb" );
    fwrite ( &( p -> head ), 1, sizeof ( p -> head ), p -> pF );
    _rMalloc ( );

    for ( UINT i = 0; i < kN_GridX; ++i )
    {
      p -> pR_S [ i ] = p -> head. kR_S_x;
    }
    p -> pR_S [ 0 ] = p -> head. kR_S_t;
    fwrite ( p -> pR_S, 1, kN_FrameSz, p -> pF );

  }
  else
  {
    p -> pF = fopen ( p -> szFileName, "r+b" );
    if ( p -> pF == NULL ) goto P_newfile;
    fread ( &( p -> head ), 1, sizeof ( p -> head ), p -> pF );
    assert ( p -> head. kI_Magic == D_MAGIC_V_1 );
    _rMalloc ( );
    fseek ( p -> pF, sizeof ( p -> head ) +
          ( kN_FrameSz * ( p -> head. kN_FrameCount ) * 2 ),
          SEEK_SET );
    fread ( p -> pR_S, 1, kN_FrameSz, p -> pF );
    assert ( p -> pR_S [ 0 ] == p -> head. kR_S_t );
  }

  rSolution_DbgHeadReWrite ( p );
}

static void rSolution_Free ( struct solution * const p )
{
  fseek ( p -> pF, 0, SEEK_SET );
  fwrite ( &( p -> head ), 1, sizeof ( p -> head ), p -> pF );
  rSolution_DbgHeadReWrite ( p );
  fclose ( p -> pF );
  _mm_free ( p -> pMemPtr );
  free ( p );
}


static void rSolution_Step0 ( struct solution * const p )
{
  double * const pR_S           = p -> pR_S;
  double * const pR_f           = p -> pR_f;
  double * const pR_Phi         = p -> pR_Phi;
  double * const pR_1_Sum_Phi   = p -> pR_1_Sum_Phi;
  const UINT     kN_GridX_1     = p -> head. kN_GridX - 1;
  const double   kR_Alpha       = p -> head. kR_Alpha;
  const double   kR_K_Mu        = p -> head. kR_K_Mu;
  for ( UINT i = 0; i < kN_GridX_1; ++i )
  {
    pR_f [ i ] = pow ( pR_S [ i ], kR_Alpha );
  }
  for ( UINT i = 0; i < kN_GridX_1; ++i )
  {
    pR_Phi [ i ] = pR_f [ i ] + kR_K_Mu * pow ( 1.0 - pR_S [ i ], kR_Alpha );
  }
  for ( UINT i = 1; i < kN_GridX_1; ++i )
  {
    pR_1_Sum_Phi [ i ] = 1.0 / ( pR_Phi [ i-1 ] + pR_Phi [ i ] );
  }
  p -> bDirty = TRUE;
}


static void rSolution_StepP ( struct solution * const p )
{
  if ( p -> bSolved ) return;
  const double          fR_P_Err_Old    = p -> fR_P_Err;
  double const * const  pR_Phi          = p -> pR_Phi;
  double * const        pR_P            = p -> pR_P;
  double const * const  pR_1_Sum_Phi    = p -> pR_1_Sum_Phi;
  const UINT            kN_GridX_1      = p -> head. kN_GridX - 1;
  double                fR_P_Err        = 0.0;
  double                fR_P_Buf;

  if ( p -> head. kN_ItersSkips && p -> nPIS )
  {
    if ( p -> nPIS <= p -> head. kN_ItersSkips )
    {
      goto P_solved;
    }
    p -> nPIS = 0;
  }

  for ( UINT i = 1; i < kN_GridX_1; ++i )
  {
    fR_P_Buf = ( pR_Phi [ i-1 ] * pR_P [ i-1 ] + pR_Phi [ i ] * pR_P [ i+1 ] ) *
            pR_1_Sum_Phi [ i ];
    fR_P_Err += fabs ( fR_P_Buf - pR_P [ i ] );
    pR_P [ i ] = fR_P_Buf;
  }
  p -> fR_P_Err = fR_P_Err;
  if ( p -> head. kN_ItersIdentMin )
  {
    if ( fR_P_Err_Old == fR_P_Err )
    {
      ++( p -> nPII );
    }
    else
    {
      p -> nPII = 0;
    }
    if ( p -> nPII >= p -> head. kN_ItersIdentMin )
    {
      goto P_solved;
    }
  }
  else
  {
    if ( fR_P_Err < p -> head. kR_ErrorMax )
    {
      goto P_solved;
    }
  }
  return;

  P_solved:
  {
    const UINT            kN_FrameSz = sizeof(double) * (p -> head. kN_GridX);
    double * const        pR_S            = p -> pR_S;
    double const * const  pR_f            = p -> pR_f;
    double * const        pR_U            = p -> pR_U;
    const double          kR_1_dX         = p -> kR_1_dX;
    const double          kR_dT_dX        = p -> kR_dT_dX;
    for ( UINT i = 0; i < kN_GridX_1; ++i )
    {
      pR_U [ i ] = kR_1_dX * pR_f [ i ] * ( pR_P [ i ] - pR_P [ i+1 ] );
    }
    for ( UINT i = 1; i < kN_GridX_1; ++i )
    {
      pR_S [ i ] += kR_dT_dX * ( pR_U [ i-1 ] - pR_U [ i ] );
    }
    FILE * const pF =  p -> pF;
    fseek ( pF, 0, SEEK_END );
    fwrite ( pR_P, 1, kN_FrameSz, pF );
    fwrite ( pR_S, 1, kN_FrameSz, pF );
    ++( p -> head. kN_FrameCount );
    // fseek ( pF, 0, SEEK_SET );
    // fwrite ( &( p -> head ), 1, sizeof ( p -> head ), pF );
    // fflush ( pF );

    if ( p -> kN_FrameCountMax )
    {
      if ( ( p -> bSolved = p -> head. kN_FrameCount > p -> kN_FrameCountMax ) )
      {
        --kN_SolCount;
      }
    }

    rSolution_Step0 ( p );
    ++(p -> nPIS);
  }

}

static void rSigTerm ( int i )
{
  kN_SolCount = 0;
  if ( g_hWnd )
  {
    DestroyWindow ( g_hWnd );
  }
}

int main ( int argc, char const *argv[] )
{
  static void* pState = &&P_begin;
  goto *pState;

  enum
  {
    kEnum_0 = 0,
    kEnum_N_GridX,
    kEnum_N_ItersIdentMin,
    kEnum_N_ItersSkips,
    kEnum_R_ErrorMax,
    kEnum_R_Alpha,
    kEnum_R_K_Mu,
    kEnum_R_dT,
    kEnum_R_S_t,
    kEnum_R_S_x,
    kEnum_N_FrameCountMax,

    kEnum_txt,
  };
  P_begin:;

  signal ( SIGTERM, rSigTerm );
  signal ( SIGINT, rSigTerm );


  if ( argc == 1 )
  {
    printf (
      "==========\n"
      "Usage: a.exe [-noGUI] [options] file [[options] file]...\n"
      "  -noGUI                 disable gui ( stop with [CTRL+C] )\n"
      "Options:\n"
      "  -W                     rewrite file\n"
      "  -new                   rewrite file\n"
      "  -G                     plot S[x]\n"
      "  -N   <unsigned>        set kN_GridX\n"
      "  -F   <unsigned>        set kN_FrameCountMax\n"
      "  -IIM <unsigned>        set kN_ItersIdentMin\n"
      "  -IS  <unsigned>        set kN_ItersSkips\n"
      "  -Err <float>           set kR_ErrorMax\n"
      "  -a   <float>           set kR_Alpha\n"
      "  -Km  <float>           set kR_K_Mu\n"
      "  -dT  <float>           set kR_dT\n"
      "  -S_t <float>           set kR_S_t\n"
      "  -S_x <float>           set kR_S_x\n"
      "  -txt <file>            write head data to txt file\n"
      "==========\n" );
    return -1;
  }
  else
  {
    struct solution * p = rSolution_New ( );
    UINT kE = kEnum_0;
    for ( UINT i = 1; i < argc; ++i )
    {
      #define D_cmp(a) (memcmp(argv[i],a,sizeof(a))==0)
      #define D_cmp_(a,b) if(memcmp(argv[i],a,sizeof(a))==0) {kE=b;}
      switch ( kE )
      {
        case kEnum_0:
          if ( D_cmp ( "-W" ) || D_cmp ( "-new" ) ) { p -> bReWrite = TRUE; } else
          if ( D_cmp ( "-G" ) ) { p -> iPlotN = kN_SolCountPlot+1; } else
          D_cmp_ ( "-N",   kEnum_N_GridX ) else
          D_cmp_ ( "-F",   kEnum_N_FrameCountMax ) else
          D_cmp_ ( "-IIM", kEnum_N_ItersIdentMin ) else
          D_cmp_ ( "-IS",  kEnum_N_ItersSkips ) else
          D_cmp_ ( "-Err", kEnum_R_ErrorMax ) else
          D_cmp_ ( "-a",   kEnum_R_Alpha ) else
          D_cmp_ ( "-Km",  kEnum_R_K_Mu ) else
          D_cmp_ ( "-dT",  kEnum_R_dT ) else
          D_cmp_ ( "-S_t", kEnum_R_S_t ) else
          D_cmp_ ( "-S_x", kEnum_R_S_x ) else
          D_cmp_ ( "-txt", kEnum_txt ) else
          if ( argv[i][0] != '-' )
          {
            strcpy ( p -> szFileName, argv[i] );
            ++kN_SolCount;
            if ( p -> iPlotN ) { ++kN_SolCountPlot; }
            if ( pS_First == NULL )
            {
              pS_First = pS_Last = p;
            }
            else
            {
              pS_Last -> pNext = p;
              p -> pPrev = pS_Last;
              pS_Last = p;
            }
            p = rSolution_New ( );
          }
          continue;
        case kEnum_txt:
          p -> szFileNameTxt = argv[i];
          kE = kEnum_0; break;
        case kEnum_N_GridX:
          p -> head. kN_GridX           = strtoul ( argv[i], NULL, 0 );
          kE = kEnum_0; break;
        case kEnum_N_FrameCountMax:
          p -> kN_FrameCountMax         = strtoul ( argv[i], NULL, 0 );
          kE = kEnum_0; break;
        case kEnum_N_ItersIdentMin:
          p -> head. kN_ItersIdentMin   = strtoul ( argv[i], NULL, 0 );
          kE = kEnum_0; break;
        case kEnum_N_ItersSkips:
          p -> head. kN_ItersSkips      = strtoul ( argv[i], NULL, 0 );
          kE = kEnum_0; break;
        case kEnum_R_ErrorMax:
          p -> head. kR_ErrorMax        = strtod ( argv[i], NULL );
          p -> head. kN_ItersIdentMin   = 0;
          kE = kEnum_0; break;
        case kEnum_R_Alpha:
          p -> head. kR_Alpha           = strtod ( argv[i], NULL );
          kE = kEnum_0; break;
        case kEnum_R_K_Mu:
          p -> head. kR_K_Mu            = strtod ( argv[i], NULL );
          kE = kEnum_0; break;
        case kEnum_R_dT:
          p -> head. kR_dT              = strtod ( argv[i], NULL );
          kE = kEnum_0; break;
        case kEnum_R_S_t:
          p -> head. kR_S_t             = strtod ( argv[i], NULL );
          kE = kEnum_0; break;
        case kEnum_R_S_x:
          p -> head. kR_S_x             = strtod ( argv[i], NULL );
          kE = kEnum_0; break;
      }
      #undef D_cmp_
      #undef D_cmp
    }
    free ( p );
  }

  // signal ( SIGTERM, rSigTerm );
  // if (funcptr == SIG_IGN) signal ( SIGTERM, SIG_IGN );

  for ( struct solution * p = pS_First; p; p = p -> pNext )
  {
    rSolution_Init ( p );
  };


  for ( struct solution * p = pS_First; p; p = p -> pNext )
  {
    rSolution_Step0 ( p );
  };

  if ( g_hWnd )
  {
    pState = &&P_while;
    return 0;
  }

  while ( kN_SolCount )
  {
    P_while:;
    for ( struct solution * p = pS_First; p; p = p -> pNext )
    {
      rSolution_StepP ( p );
    };
    if ( g_hWnd )
    {
      if ( kN_SolCount == 0 )
      {
        pState = &&P_end;
      }
      return 0;
    }
  }

  P_end:;

  struct solution * p1;
  for ( struct solution * p = pS_Last; p; p = p1 )
  {
    p1 = p -> pPrev;
    rSolution_Free ( p );
  };
  return 0;
}




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

  {
    INT iOut = main ( g_nArgs, (LPCSTR*)g_szArgList );
    if ( iOut ) return iOut;
  }

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

  glGenVertexArrays ( 1, &g_iVAO );
  glBindVertexArray ( g_iVAO );
  glGenBuffers ( 1, &g_iVBO );
  glBindBuffer ( GL_ARRAY_BUFFER, g_iVBO );
  // glBufferData ( GL_ARRAY_BUFFER, (kN_GridX*4) * sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW );
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0 );
  glEnableVertexAttribArray ( 0 );

  return 0;
}

static LRESULT rOnDestroy ( )
{
  kN_SolCount = 0;
  main ( 0, NULL );
  main ( 0, NULL );
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
  g_nWndWidth = nWidth;
  g_nWndHeight = nHeight;
  glViewport ( 0, 0, nWidth, nHeight );
  return 0;
}

static VOID rOnIdle ( )
{
  main ( 0, NULL );

  if ( kN_SolCountPlot )
  {
    BOOL bN = FALSE;
    for ( struct solution * p = pS_First; p; p = p -> pNext )
    {
      if ( p -> iPlotN && p -> bDirty ) { bN = TRUE; break; }
    }
    if ( bN )
    {
      glClearColor ( 0.0, 0.0, 0.0, 1.0 );
      glClear ( GL_COLOR_BUFFER_BIT );
      for ( struct solution * p = pS_First; p; p = p -> pNext )
      {
        if ( p -> iPlotN )
        {
          glViewport ( 0, g_nWndHeight * (p -> iPlotN-1) / kN_SolCountPlot,
                  g_nWndWidth, g_nWndHeight / kN_SolCountPlot );
          const UINT kN_GridX = p -> head . kN_GridX;
          const double kR_dX = p -> kR_dX;
          GLfloat f[ kN_GridX * 4 ];
          for ( UINT i = 0; i < kN_GridX; ++i )
          {
            f[i*4+2] = f[i*4+0] = i * kR_dX;
            f[i*4+1] = 0.0;
            f[i*4+3] = p -> pR_S[i];
          }
          glBufferData ( GL_ARRAY_BUFFER, (kN_GridX*4) * sizeof(GLfloat), f, GL_DYNAMIC_DRAW );
          glDrawArrays ( GL_TRIANGLE_STRIP, 0, kN_GridX*2 );
          p -> bDirty = FALSE;
        }
      }
      SwapBuffers ( g_hWndDC );
    }
  }

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
  {
    LPWSTR *szArglist;
    g_szArgList = (LPSTR*)( szArglist = CommandLineToArgvW ( GetCommandLineW ( ), &g_nArgs ) );
    for ( UINT i = 0; i < g_nArgs; ++i )
    {
      LPSTR pSrc = g_szArgList[i];
      LPWSTR pDst = szArglist[i];
      while ( *pDst )
      {
        *pSrc = *pDst;
        ++pSrc; ++pDst;
      }
      *pSrc = *pDst;
      printf ( "%s\n", g_szArgList[i] );
    }
  }

  if ( g_nArgs >= 2 && memcmp ( g_szArgList[1], "-noGUI", sizeof("-noGUI") ) == 0 )
  {
    INT iOut = main ( g_nArgs, (LPCSTR*)g_szArgList );
    LocalFree ( g_szArgList );
    return iOut;
  }



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
  if ( !g_hWnd ) goto P_End;
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
  LocalFree ( g_szArgList );
  UnregisterClass ( g_cszClassName, g_hInstance );
  FreeLibrary ( g_hModule_OpenGL );
  return msg.wParam;
}

