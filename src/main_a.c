#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <windows.h>

#include <intrin.h>

#define D_MAGIC_V_1             0xE35AB12DE35AB12DULL

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
  BOOL                          bReWrite;
  BOOL                          bSolved;
};

static struct solution         *pS_First                    = NULL;
static struct solution         *pS_Last                     = NULL;
static UINT                     kN_SolCount                 = 0;

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
  p -> head. kR_dT              = 1e-1;
  p -> head. kR_S_t             = 1.0;
  p -> head. kR_S_x             = 0.0;
  return p;
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

    for ( UINT i = 0; i < kN_GridX; ++i )
    {
      p -> pR_P [ i ] = 1.0 - ( ((double)i) * ( p -> kR_dX ) );
    }
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

  if ( p -> szFileNameTxt )
  {
    FILE * const pF = fopen ( p -> szFileNameTxt, "wb" );
    #define D_print(...) fprintf ( pF, __VA_ARGS__ )
    D_print ( "kI_Magic             = %08X\n", p -> head. kI_Magic );
    D_print ( "kN_FrameCount        = %d\n", p -> head. kN_FrameCount );
    D_print ( "kN_GridX             = %d\n", p -> head. kN_GridX );
    D_print ( "kN_ItersIdentMin     = %d\n", p -> head. kN_ItersIdentMin );
    D_print ( "kN_ItersSkips        = %d\n", p -> head. kN_ItersSkips );
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
    D_print ( "======================\n" );
    #undef D_print
    fclose ( pF );
  }
}

static void rSolution_Free ( struct solution * const p )
{
  fseek ( p -> pF, 0, SEEK_SET );
  fwrite ( &( p -> head ), 1, sizeof ( p -> head ), p -> pF );
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
    // fseek ( pF, 0, SEEK_END );
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
  }

}

static void rSigTerm ( int i )
{
  struct solution * p1;
  for ( struct solution * p = pS_Last; p; p = p1 )
  {
    p1 = p -> pPrev;
    rSolution_Free ( p );
  };
  exit ( 0 );
}

int main ( int argc, char const *argv[] )
{
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
  if ( argc == 1 )
  {
    printf (
      "==========\n"
      "Usage: a.exe [options] file [[options] file]...\n"
      "Options:\n"
      "  -W                     rewrite file\n"
      "  -new                   rewrite file\n"
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

  signal ( SIGTERM, rSigTerm );
  // if (funcptr == SIG_IGN) signal ( SIGTERM, SIG_IGN );

  for ( struct solution * p = pS_First; p; p = p -> pNext )
  {
    rSolution_Init ( p );
  };


  for ( struct solution * p = pS_First; p; p = p -> pNext )
  {
    rSolution_Step0 ( p );
  };

  while ( kN_SolCount )
  {
    for ( struct solution * p = pS_First; p; p = p -> pNext )
    {
      rSolution_StepP ( p );
    };
  }



  struct solution * p1;
  for ( struct solution * p = pS_Last; p; p = p1 )
  {
    p1 = p -> pPrev;
    rSolution_Free ( p );
  };
  return 0;
}

