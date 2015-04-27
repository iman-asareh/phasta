//MR CHANGE
#define OMPI_SKIP_MPICXX 1
//MR CHANGE END
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_PETSC
#include <petscsys.h>
#include <petscviewer.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include "common_c.h"

#if !(defined IOSTREAMH)
#include <iostream>
#include <sstream>
using namespace std;
#endif

#include <FCMangle.h>
#define input FortranCInterface_GLOBAL_(input,INPUT)
#define proces FortranCInterface_GLOBAL_(proces,PROCES)
#define timer FortranCInterface_GLOBAL_(timer,TIMER)

#ifdef intel
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

extern "C" char phasta_iotype[80];
char phasta_iotype[80];

extern int SONFATH;
extern void Partition_Problem( int, char[], char[], int );
extern "C" void proces();
extern "C" void input();
extern int input_fform(char inpfname[]);
//MR CHANGE
extern void setIOparam(); // For SyncIO
//MR CHANGE END

int myrank; /* made file global for ease in debugging */

void
catchDebugger() {
    while (1) { 
      int debuggerPresent=0;
      int fakeSTOP = 1; // please stop HERE and assign as next line 
      // assign or set debuggerPresent=1
      if(debuggerPresent) {
        break;
      }
    }
}

// some useful debugging functions

void
pdarray( void* darray , int start, int end ) {
    for( int i=start; i < end; i++ ){
        cout << ((double*)darray)[i] << endl;
    }
}

void
piarray( void* iarray , int start, int end ) {
    for( int i=start; i < end; i++ ){
        cout << ((int*)iarray)[i] << endl;
    }
}

#ifdef __bgq__
    extern "C" void print_bgq_net_info();
#endif


extern "C" int 
phasta( int argc,   
        char *argv[] ) {
  
    int size,ierr;
    char inpfilename[100];
    char* pauseDebugger = getenv("catchDebugger");
    //cout << "pauseDebugger" << pauseDebugger << endl;

    MPI_Init(&argc,&argv);
    MPI_Comm_size (MPI_COMM_WORLD, &size);
    MPI_Comm_rank (MPI_COMM_WORLD, &myrank);
    MPI_Barrier(MPI_COMM_WORLD);
    if(myrank == 0)
    {
	    printf("About to init PETSc\n");
	    fflush(stdout);
    }
#ifdef __bgq__
    print_bgq_net_info();
#endif

#ifdef HAVE_PETSC
    PETSC_COMM_WORLD=MPI_COMM_WORLD;
    PetscInitialize(&argc,&argv,PETSC_NULL,PETSC_NULL);
    PetscInitializeFortran();
    PetscPopSignalHandler(); //Let us segfault in peace ;-)
    PetscViewerSetFormat(PETSC_VIEWER_STDOUT_WORLD, PETSC_VIEWER_ASCII_INDEX);
    MPI_Barrier(MPI_COMM_WORLD);
    if(myrank == 0)
    {
	    printf("PETSc Initialized\n");
	    fflush(stdout);
    }
#endif
    workfc.numpe = size;
    workfc.myrank = myrank;

#if (defined WIN32)
    if(argc > 2 ){
      catchDebugger();
    }
#endif
#if (1) // ALWAYS ( defined LAUNCH_GDB ) && !( defined WIN32 )

    if ( pauseDebugger ) {

        int parent_pid = getpid();
        int gdb_child = fork();
        cout << "gdb_child" << gdb_child << endl;

        if( gdb_child == 0 ) {
     
            cout << "Debugger Process initiating" << endl;
            stringstream exec_string;
         
#if ( defined decalp )
            exec_string <<"xterm -e idb " 
                        << " -pid "<< parent_pid <<" "<< argv[0] << endl;
#endif
#if ( defined LINUX )
            exec_string <<"xterm -e gdb"
                        << " -pid "<< parent_pid <<" "<< argv[0] << endl;
#endif
#if ( defined SUN4 )
            exec_string <<"xterm -e dbx " 
                        << " - "<< parent_pid <<" "<< argv[0] << endl;
#endif
#if ( defined IRIX )
            exec_string <<"xterm -e dbx " 
                        << " -p "<< parent_pid <<" "<< argv[0] << endl;
#endif

			system( exec_string.str().c_str() ); //check for dangling pointer
            exit(0);
        }
        catchDebugger();
    }

#endif

    /* Input data  */
    if(argc > 1 ){
        strcpy(inpfilename,argv[1]);
    } else {
        strcpy(inpfilename,"solver.inp");
    }
    ierr = input_fform(inpfilename);
    if(!ierr){

        /* Preprocess data and run the problem  */
        /* Partition the problem to the correct number of processors */
        if( size > 1 ) {
            if( myrank == 0 ) {
                 Partition_Problem( size, phasta_iotype, 
                                    phasta_iotype, SONFATH );
                 MPI_Barrier(MPI_COMM_WORLD);
            } else { 
                 MPI_Barrier(MPI_COMM_WORLD);
                 sprintf(inpfilename,"%d-procs_case/",size);
                 if( chdir( inpfilename ) ) {
                    cerr << "could not change to the problem directory "
                              << inpfilename << endl;
                    return 1;
                 }
            }
        }

//MR CHANGE
        setIOparam();
//MR CHANGE END

        input();
        /* now we can start the solver */
        proces();
    }
    else{
        printf("error during reading ascii input \n");
    }
#ifdef HAVE_PETSC
    PetscFinalize();
#endif
    MPI_Finalize();
    return 0;
}
