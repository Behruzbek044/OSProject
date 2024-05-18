# Paging Implementation in Xv6

Our project utilizes the Xv6 OS kernel implementation provided by <https://github.com/mit-pdos/xv6-riscv>, to whom we extend our gratitude. This project enhances the memory management unit of the Xv6 kernel through the introduction of demand paging support. We have developed and evaluated a variety of page replacement strategies.

## Running the code:
```shell
sudo apt install qemu qemu-system-x86
sudo apt install libc6:i386
```
Also make sure `git`, `make` and `gcc` is installed:
```shell
sudo apt install git make gcc
```
Next
```shell
git clone https://github.com/Behruzbek044/OSProject xv6
sudo chmod 700 -R xv6
cd xv6
make clean
make qemu SELECTION=FLAG1 VERBOSE_PRINT=FLAG2
```

# Implementation Details

## `struct`s:

**freepg**: 

  - Node of linked lists of pages (max = 15) present in the main memory and the swap space.
    ```c
    struct freepg {
      char *va;
      int age;
      struct freepg *next;
      struct freepg *prev;
      uint swaploc;
    };
    ```

**proc**:

  - Added arrays of freepg for both main memory and physical memory.
  - Added other metadata like # of swaps, # of pages in main memory and swap space, # page faults.
    ```c
    // Per-process state
    struct proc {
      uint sz;                     // Size of process memory (bytes)
      pde_t* pgdir;                // Page table
      char *kstack;                // Bottom of kernel stack for this process
      enum procstate state;        // Process state
      int pid;                     // Process ID
      struct proc *parent;         // Parent process
      struct trapframe *tf;        // Trap frame for current syscall
      struct context *context;     // swtch() here to run process
      void *chan;                  // If non-zero, sleeping on chan
      int killed;                  // If non-zero, have been killed
      struct file *ofile[NOFILE];  // Open files
      struct inode *cwd;           // Current directory
      char name[16];               // Process name (debugging)

      struct file *swapFile;

      int main_mem_pages;
      int swap_file_pages;
      int page_fault_count;
      int page_swapped_count;
      
      struct freepg free_pages[MAX_PSYC_PAGES];
      struct freepg swap_space_pages[MAX_PSYC_PAGES];
      struct freepg *head;
      struct freepg *tail; 
    };
    ```

## Functions changed:

  - `allocproc(void)` and `exec()`: allocproc is responsible for searching an empty process entry in ptable array while exec is responsible for executing it. Changes done: Initialized all the process meta data to 0 and all the addresses to 0xffffffff. If the NONE flag is not defined, exec creates a swap file for every process other than init and sh.

  - `allocuvm()`: Responsile for allocating the memory for the process. Changes done: while allocating changes, tracks if the number of pages in the physical memory exceeds 15. If so, calls the required paging scheme through the writePagesToSwapFile function and later updates the number of pages in the processes' meta data using the recordNewPage function. Also called in `sbrk()` system call to allow a process to allocate its own pages.

  - `deallocuvm()`: deallocates from the physical memory and frees both the free_pages and the swap_file_pages arrays of the process it is called for. Decreaments the counts of pages in both main memory and swap space. Also called by `sbrk()` system call when supplied with negtive # of pages to allow a process to deallocate its own pages.

  - `fork() system call`: Copies the meta data of a process including swap_space_pages and free_pages arrays, # of pages in swap file and main memory to the child process. However, # page faults and # page swaps are not copied to the child. Creates a separate swap file for the child process to hold copies of the swapped out pages. This is done to enable **copy-on-write**. The codelet for making swap file for child process is:
  ```c
  #ifndef NONE
    if(curproc->pid > 2)
    {
      createSwapFile(np);
      char buf[PGSIZE/2] = "";
      int offset = 0;
      int nread = 0;
      while((nread == readFromSwapFile(curproc,buf, offset, PGSIZE/2))!=0)
        if(writeToSwapFile(np, buf, offset, nread) == -1){
          panic("fork: error while copying the parent's swap file to the child");
        offset +=nread;
      }
    }
  ```

  - `exit()` **system call**: Deletes the meta data associated with the process. Closes the swap file and removes the pointers. Sets all the # entries to 0.
  ```c
  #ifndef NONE
    if(curproc->pid >2 &&  curproc->swapFile!=0 && curproc->swapFile->ref > 0)
    {
      removeSwapFile(curproc);
    }
  #endif

  #if TRUE
    if(cuscmp(curproc->name,"sh") != 0)
      custom_proc_print(curproc);
  #endif
  ```

  - `procdump()`: Modified with `custom_proc_print()` function to print the counts of pages in swap space and in main memory, page faults, number of swaps and percentage of free pages in the physical memory.

  - `trap(struct trapframe *tf)`: Modified to call `updateNFUState()` function in case of timer interrupts to increase the "age" of pages when NFU paging scheme is being used. Introduced changes to handle a page fault by defining T_PGFLT for trap 14. On T_PGFLT being called, it executes the supplied service routine handled by the swapPages() function. The codelet for handling pagefault is as follows:
  ```c
  case T_PGFLT:
    addr = rcr2();
    vaddr = &(myproc()->pgdir[PDX(addr)]);
    if(((int)(*vaddr) & PTE_P)!=0){
      if(((uint*)PTE_ADDR(P2V(*vaddr)))[PTX(addr)] & PTE_PG){
        //cprintf("called T_PGFLT\n");
        swapPages(PTE_ADDR(addr));
        ++myproc()->page_fault_count;
        return;
      }
    }
  ```

## New functions created:

  - `WritePagesToSwapFile(char *va)`: This function calls the required paging scheme (FIFO, NFU, SCFIFO) depending upon the flag that was set during make. Incoming page is written into the physical memory and a candidate page (selected according to the paging scheme) is removed from the main memory and wriiten to the swap space.

  - `recoredNewPage(char *va)`: Writes the meta-data of the currently added page into the corresponding entry in the free_pages array of the process. Increases the count of pages in the physical memory.

  - `swapPages(char *va)`: Fetches the page with the given virtual address "va" from the swap space and finds a candidate page to be swapped out of the main memory and into the swap space based on (FIFO, NFU, SCFIFO) according to the flag set during make.

  - `printStats()` and `procDump()` system calls: Print the details of the current process and all current processes respectively as per asked by the **Enhanced Details Viewer** section. Call `custom_proc_print()` and `procDump()` functions respectively. Used in myMemTest.c to print the results and do away with `ctrl+P` during execution.
