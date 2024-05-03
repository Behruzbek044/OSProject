#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"

#define PAGESIZE 4096


int
main(int argc, char *argv[]){

#if NFU
    int i,j;
    char *page[15];
    printf(1, "\n   ***Testing NFU***\n");
    printf(1, "\n   **Process Starts Executing**\n");
    printf(1,"\nInitial process details:\n");
    printStats();


    printf(1,"\n    **Adding 12 more pages to the process**\n\n");
    //We already have 3 pages for the process in the main memory.
    //We will now allocate 12 more pages to get to the 15 page limit that we had set
    for(i = 0; i<12; i++){
        page[i] = sbrk(PAGESIZE);
        printf(1,"page[%d]=0x%x\n",i,page[i]);
    }
    printf(1,"\nProcess details:\n");
    printStats();

    //Adding one more page i.e page no.15 
    printf(1,"\n    **Allocating 16th page for the process i.e page no. 15**\n\n");
    page[12] = sbrk(PAGESIZE);
    printf(1, "page[12]=0x%x\n", page[13]);
    printf(1,"\nProcess details:\n");
    printStats();

//
    
    printf(1,"\n    **Allocating 17th page for the process ie page no. 16**\n\n");
	page[13] = sbrk(PAGESIZE);
	printf(1, "page[13]=0x%x\n", page[13]);
	printf(1,"\nProcess details:\n");
    printStats();

//


    printf(1,"\n    ** Writing to page 3**\n\n");
    page[0][0] = 'a';
    printf(1,"\nProcess details:\n");
    printStats();

//
    
    printf(1,"\n    **Allocating 18th page for the process ie page no. 17**\n\n");
	page[14] = sbrk(PAGESIZE);
	printf(1, "page[14]=0x%x\n", page[14]);
	printf(1,"\nProcess details:\n");
    printStats();

//

   // Trying to cause a page fault by accessing the 5th page i.e page no. 4 which is in swap space
    printf(1,"\n    **Writing to pages 4 to 8 which causes page fault for every page**\n\n");
    for (i = 1; i < 6; i++) {
		printf(1, "Writing to address 0x%x\n", page[i]);
		for (j = 0; j < PAGESIZE; j++){
			page[i][j] = 'k';
		}
	}
    printf(1,"\nProcess details:\n");
    printStats();

//

    printf(1,"\n    ***Testing for fork()***\n");
    if(fork() == 0){
        printf(1,"\n    **Running the child process now, PID: %d**\n",getpid());
        printf(1,"\nDetails of all processes\n");
        procDump();
        page[6][0] = 'J';
        printf(1,"\n    **Accessing page number 8 which is in the swap space**\n");
        printf(1,"\nDetails of all processes\n");
        procDump();
        exit();
    }
    else{
        wait();
        printf(1,"\n    **Child has exited. Back to the parent process now**\n");
        printf(1,"\n    **Deallocating all pages from the parent process**\n");
        sbrk(-15*PAGESIZE);
        printf(1,"\nDetails of all processes\n");
        procDump();
        exit();   
    }
//FIFO testing

#else
    #if FIFO
        printf(1,"  ***Testing for FIFO***\n");
    #elif SCFIFO
        printf(1,"  ***Testing for SCFIFO***\n");
    #endif
    printf(1,"\n    **Process Starts Executing**\n");
    int i,j;
    char *page[14];
    printf(1,"\nInitial process details:\n");
    printStats();
    printf(1,"\n    **Adding 12 more pages to the process**\n\n");
    //We already have 3 pages for the process in the main memory.
    //We will now allocate 12 more pages to get to the 15 page limit that we had set
    for(i = 0; i<12; i++){
        page[i] = sbrk(PAGESIZE);
        printf(1,"page[%d]=0x%x\n",i,page[i]);
    }
    printf(1,"\nProcess details:\n");
    printStats();

    // Now

    
    //Adding one more page should replace page number 0 in memory with 12. 
    printf(1,"\n    **Allocating 16th page for the process i.e page no. 15**\n\n");
    page[12] = sbrk(PAGESIZE);
    printf(1,"page[%d]=0x%x\n",12,page[12]);
    //sbrk returning 61440 but below statement not getting called
    printf(1,"\nProcess details:\n");
    printStats();

//

   
    printf(1,"\n    **Allocating 17th page for the process i.e page no. 16**\n\n");
    page[13] = sbrk(PAGESIZE);
    printf(1,"page[13]=0x%x",page[13]);
    printf(1,"\nProcess details:\n");
    printStats();

//


    printf(1,"\n    ** Writing to page 3**\n\n");
    page[0][0] = 'a';
    printf(1,"\nProcess details:\n");
    printStats();

//
    

    printf(1,"\n    **Allocating 18th page for the process i.e page no. 17**\n\n");
    page[14] = sbrk(PAGESIZE);
    printf(1,"page[14]=0x%x",page[14]);
    printf(1,"\nProcess details:\n");
    printStats();


//

    printf(1,"\n    **Trying to cause 6 more page faults by using pages from the swapspace**\n\n");
    for(i=1; i<6; i++){
        printf(1, "Writing to address 0x%x\n", page[i]);
        for(j=0; j<PAGESIZE; j++)
            page[i][j] = 'S';
    }
    printf(1,"\nProcess details:\n");
    printStats();

//


    printf(1,"\n    ***Testing for fork()***\n");
    if(fork() == 0){
        printf(1,"\n    **Running the child process now, PID: %d**\n",getpid());
        printf(1,"\nDetails of all processes\n");
        procDump();
        page[7][0] = 'J';
        printf(1,"\n    **Accessing page number 9 which is in swap space**\n");
        printf(1,"\nDetails of all processes\n");
        procDump();
        exit();
    }
    else{
        wait();
        printf(1,"\n    **Child has exited. Back to the parent process now**\n");
        printf(1,"\n    **Deallocating all pages from the parent process**\n");
        sbrk(-15*PAGESIZE);
        printf(1,"\nDetails of all processes\n");
        procDump();
        exit();   
    }
#endif

    return 0;
}
