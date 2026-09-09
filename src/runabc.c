/**CFile****************************************************************

  FileName    [runabc.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [ABC as a static library.]

  Synopsis    [Read an AIG as MiniAIG, map it, and export MiniLUT.]

  Description [This program illustrates embedding ABC as a static library.
               It reads an AIGER file directly into the lightweight MiniAIG
               representation, starts an ABC frame, and imports the MiniAIG
               into that frame.  It then runs the ABC command "if -K 6" to
               map the network into 6-input LUTs.  Finally, it exports the
               mapped network as a MiniLUT, prints the MiniLUT statistics,
               releases all data structures, and stops the ABC frame.  The
               optional threaded test runs this flow concurrently using one
               explicitly created ABC frame per thread and compares each
               complete MiniLUT result with a single-thread baseline.

               The input filename is optional.  If none is supplied, the
               program reads "i10.aig" from the current directory.

               Build and run from the root of the ABC distribution:

                 make libabc.a
                 gcc -Wall -O2 -Isrc -c src/runabc.c -o runabc.o
                 g++ -o runabc runabc.o libabc.a \
                     -lreadline -lpthread -lm -ldl -lrt
                 ./runabc
                 ./runabc path/to/network.aig
                 ./runabc -t 4 path/to/network.aig

               The link libraries shown above match a default Linux build.
               On macOS, omit -lrt.  If ABC is built without readline using
               "make ABC_USE_NO_READLINE=1 libabc.a", omit -lreadline.

               Example transcript using the default input:

                 $ make libabc.a
                 Linking: libabc.a
                 $ gcc -Wall -O2 -Isrc -c src/runabc.c -o runabc.o
                 $ g++ -o runabc runabc.o libabc.a \
                       -lreadline -lpthread -lm -ldl -lrt
                 $ ./runabc
                 PI = 257. PO = 224. LUT = 612. FF = 0.]

***********************************************************************/

#include "misc/util/abc_namespaces.h"
#include "aig/miniaig/miniaig.h"
#include "aig/miniaig/minilut.h"

#if defined(_WIN32) && !defined(__MINGW32__)
#include "../lib/pthread.h"
#else
#include <pthread.h>
#endif
#include <stdint.h>

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

ABC_NAMESPACE_HEADER_START

typedef struct Abc_Frame_t_ Abc_Frame_t;
Abc_Frame_t * Abc_FrameCreate( void );
void          Abc_FrameDestroy( Abc_Frame_t * pAbc );
Abc_Frame_t * Abc_FrameEnter( Abc_Frame_t * pAbc );
void          Abc_FrameLeave( Abc_Frame_t * pPrevious );
void          Abc_NtkInputMiniAig( Abc_Frame_t * pAbc, void * pMiniAig );
void *        Abc_FrameGiaOutputMiniLut( Abc_Frame_t * pAbc );
int           Cmd_CommandExecute( Abc_Frame_t * pAbc, const char * pCommandLine );

ABC_NAMESPACE_HEADER_END
ABC_NAMESPACE_USING_NAMESPACE

typedef struct Run_Result_t_
{
    int nPis;
    int nPos;
    int nLuts;
    int nFfs;
    int nObjs;
    int LutSize;
    int Status;
    uint64_t Hash;
} Run_Result_t;

typedef struct Run_Group_t_
{
    pthread_mutex_t Mutex;
    pthread_cond_t Cond;
    int nReady;
    int fStart;
} Run_Group_t;

typedef struct Run_Thread_t_
{
    pthread_t Thread;
    Run_Group_t * pGroup;
    const char * pFileName;
    Run_Result_t Result;
} Run_Thread_t;

static uint64_t Run_HashAdd( uint64_t Hash, unsigned Value )
{
    int i;
    for ( i = 0; i < 4; i++ )
    {
        Hash ^= (unsigned char)(Value >> (8 * i));
        Hash *= UINT64_C(1099511628211);
    }
    return Hash;
}

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

static int Run_One( const char * pFileName, Run_Result_t * pResult )
{
    Abc_Frame_t * pAbc = NULL;
    Abc_Frame_t * pPrevious = NULL;
    Mini_Aig_t * pMiniAig = NULL;
    Mini_Lut_t * pMiniLut = NULL;
    int i;

    memset( pResult, 0, sizeof(Run_Result_t) );
    pMiniAig = Mini_AigerRead( (char *)pFileName, 0 );
    if ( pMiniAig == NULL )
    {
        pResult->Status = 1;
        return 1;
    }
    pAbc = Abc_FrameCreate();
    if ( pAbc == NULL )
    {
        pResult->Status = 2;
        goto cleanup;
    }
    pPrevious = Abc_FrameEnter( pAbc );
    Abc_NtkInputMiniAig( pAbc, pMiniAig );
    Mini_AigStop( pMiniAig );
    pMiniAig = NULL;

    if ( Cmd_CommandExecute( pAbc, "if -K 6" ) )
    {
        pResult->Status = 3;
        goto cleanup;
    }
    if ( Cmd_CommandExecute( pAbc, "&get -m" ) )
    {
        pResult->Status = 4;
        goto cleanup;
    }
    pMiniLut = (Mini_Lut_t *)Abc_FrameGiaOutputMiniLut( pAbc );
    if ( pMiniLut == NULL )
    {
        pResult->Status = 5;
        goto cleanup;
    }
    Mini_LutForEachPi( pMiniLut, i )
        pResult->nPis++;
    Mini_LutForEachPo( pMiniLut, i )
        pResult->nPos++;
    Mini_LutForEachNode( pMiniLut, i )
        pResult->nLuts++;
    pResult->nFfs = pMiniLut->nRegs;
    pResult->nObjs = pMiniLut->nSize;
    pResult->LutSize = pMiniLut->LutSize;
    pResult->Hash = UINT64_C(1469598103934665603);
    for ( i = 0; i < pMiniLut->nSize * pMiniLut->LutSize; i++ )
        pResult->Hash = Run_HashAdd( pResult->Hash, (unsigned)pMiniLut->pArray[i] );
    for ( i = 0; i < pMiniLut->nSize * Mini_LutWordNum(pMiniLut->LutSize); i++ )
        pResult->Hash = Run_HashAdd( pResult->Hash, pMiniLut->pTruths[i] );
    Mini_LutStop( pMiniLut );
    pMiniLut = NULL;

cleanup:
    if ( pMiniLut )
        Mini_LutStop( pMiniLut );
    if ( pMiniAig )
        Mini_AigStop( pMiniAig );
    if ( pAbc )
    {
        Abc_FrameLeave( pPrevious );
        Abc_FrameDestroy( pAbc );
    }
    return pResult->Status != 0;
}

static void Run_PrintResult( const char * pPrefix, Run_Result_t * pResult )
{
    printf( "%sPI = %d. PO = %d. LUT = %d. FF = %d. Hash = %016llx.\n",
        pPrefix, pResult->nPis, pResult->nPos, pResult->nLuts, pResult->nFfs,
        (unsigned long long)pResult->Hash );
}

static int Run_ResultIsEqual( Run_Result_t * pResult0, Run_Result_t * pResult1 )
{
    return pResult0->Status  == pResult1->Status  &&
           pResult0->nPis    == pResult1->nPis    &&
           pResult0->nPos    == pResult1->nPos    &&
           pResult0->nLuts   == pResult1->nLuts   &&
           pResult0->nFfs    == pResult1->nFfs    &&
           pResult0->nObjs   == pResult1->nObjs   &&
           pResult0->LutSize == pResult1->LutSize &&
           pResult0->Hash    == pResult1->Hash;
}

static void * Run_ThreadWorker( void * pArg )
{
    Run_Thread_t * pThread = (Run_Thread_t *)pArg;
    pthread_mutex_lock( &pThread->pGroup->Mutex );
    pThread->pGroup->nReady++;
    pthread_cond_broadcast( &pThread->pGroup->Cond );
    while ( !pThread->pGroup->fStart )
        pthread_cond_wait( &pThread->pGroup->Cond, &pThread->pGroup->Mutex );
    pthread_mutex_unlock( &pThread->pGroup->Mutex );
    Run_One( pThread->pFileName, &pThread->Result );
    return NULL;
}

static int Run_ThreadedTest( const char * pFileName, int nThreads )
{
    Run_Result_t Baseline;
    Run_Group_t Group;
    Run_Thread_t * pThreads;
    int i, nCreated = 0, RetValue = 0;

    if ( Run_One(pFileName, &Baseline) )
    {
        printf( "Single-thread baseline failed with status %d.\n", Baseline.Status );
        return 1;
    }
    Run_PrintResult( "Baseline: ", &Baseline );
    memset( &Group, 0, sizeof(Run_Group_t) );
    pthread_mutex_init( &Group.Mutex, NULL );
    pthread_cond_init( &Group.Cond, NULL );
    pThreads = (Run_Thread_t *)calloc( nThreads, sizeof(Run_Thread_t) );
    for ( i = 0; i < nThreads; i++ )
    {
        pThreads[i].pGroup = &Group;
        pThreads[i].pFileName = pFileName;
        if ( pthread_create(&pThreads[i].Thread, NULL, Run_ThreadWorker, &pThreads[i]) )
            break;
        nCreated++;
    }
    pthread_mutex_lock( &Group.Mutex );
    while ( Group.nReady < nCreated )
        pthread_cond_wait( &Group.Cond, &Group.Mutex );
    Group.fStart = 1;
    pthread_cond_broadcast( &Group.Cond );
    pthread_mutex_unlock( &Group.Mutex );
    for ( i = 0; i < nCreated; i++ )
        pthread_join( pThreads[i].Thread, NULL );
    if ( nCreated != nThreads )
    {
        printf( "Created only %d of %d requested threads.\n", nCreated, nThreads );
        RetValue = 1;
    }
    for ( i = 0; i < nCreated; i++ )
        if ( !Run_ResultIsEqual(&Baseline, &pThreads[i].Result) )
        {
            printf( "Thread %d does not match the baseline.\n", i );
            Run_PrintResult( "Thread result: ", &pThreads[i].Result );
            RetValue = 1;
        }
    if ( RetValue == 0 )
        printf( "All %d threaded runs match the baseline.\n", nThreads );
    free( pThreads );
    pthread_cond_destroy( &Group.Cond );
    pthread_mutex_destroy( &Group.Mutex );
    return RetValue;
}

int main( int argc, char ** argv )
{
    const char * pFileName = "i10.aig";
    Run_Result_t Result;
    int nThreads = 0;

    if ( argc > 1 && !strcmp(argv[1], "-t") )
    {
        if ( argc < 3 || argc > 4 || (nThreads = atoi(argv[2])) < 1 || nThreads > 64 )
            goto usage;
        if ( argc == 4 )
            pFileName = argv[3];
        return Run_ThreadedTest( pFileName, nThreads );
    }
    if ( argc > 2 )
        goto usage;
    if ( argc == 2 )
        pFileName = argv[1];
    if ( Run_One(pFileName, &Result) )
    {
        printf( "ABC flow failed with status %d.\n", Result.Status );
        return 1;
    }
    printf( "PI = %d. PO = %d. LUT = %d. FF = %d.\n",
        Result.nPis, Result.nPos, Result.nLuts, Result.nFfs );
    return 0;

usage:
    printf( "usage: %s [file.aig]\n", argv[0] );
    printf( "       %s -t threads [file.aig]\n", argv[0] );
    return 1;
}
