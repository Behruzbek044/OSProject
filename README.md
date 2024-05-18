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

  - This is a node for linked lists of pages (up to 15) that are present in both the main memory and the swap space.
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

  - This struct has been updated to include **arrays of freepg** for both **main memory** and **physical memory**, along with additional metadata such as **the number of swaps**, **number of pages** in main memory and swap space, and **the number of page faults**.
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

## Modified Functions:

  - `allocproc(void)` and `exec()`: `allocproc()` is responsible for searching an empty process entry in `ptable` array while `exec()` is responsible for executing it. Changes: These functions now initialize all the process meta data to 0 and all the addresses to 0xffffffff. If the NONE flag is not defined, `exec()` creates a swap file for every process other than init and sh.

  - `allocuvm()`: This function, responsible for allocating memory for the process, now tracks if the number of pages in physical memory exceeds 15. If it does, it calls the appropriate paging scheme through the writePagesToSwapFile function and updates the number of pages in the process's metadata using the recordNewPage function.

  - `deallocuvm()`: deallocates from the physical memory and frees both the `free_pages` and the `swap_file_pages` arrays of the process it is called for. Decreaments the counts of pages in both main memory and swap space. Also called by `sbrk()` system call, when supplied with negative # of pages to allow a process to deallocate its own pages.

  - `fork()` **system call**: This function now copies the metadata of a process, including `swap_space_pages` and `free_pages` arrays, number of pages in swap file and main memory, to the child process, but does not copy the number of page faults and page swaps to the child process. It also creates **a separate swap** file for the child process to hold copies of the swapped out pages. This is done to enable **copy-on-write**.
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

  - `exit()` **system call**: This function now **deletes** the metadata associated with the process, closes the swap file, and removes the pointers. It sets all the number entries to 0.
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

  - `procdump()`: Modified to `custom_proc_print()` function to print the counts of pages in swap space and in main memory, page faults, number of swaps, and percentage of free pages in physical memory.

  - `trap(struct trapframe *tf)`: Modified to call `updateNFUState()` function in case of timer interrupts **to increase** the "age" of pages when NFU paging is used, and **to handle** a page fault by defining T_PGFLT for trap 14. When T_PGFLT is called, it executes the supplied service routine handled by the `swapPages()` function.
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

## New Functions:

  - `WritePagesToSwapFile(char *va)`: Calls the required paging algorithm (FIFO, NFU, SCFIFO) depending on the flag that was set during `make`, Writes the incoming page into the physical memory and removes a candidate page (selected according to the paging scheme) from the main memory, writing it to the swap space.

  - `recoredNewPage(char *va)`: Writes the metadata of the currently added page into the corresponding entry in the `free_pages` array of the process, Increases the count of pages in the physical memory.

  - `swapPages(char *va)`: Retrieves the page with the given virtual address `va` from the swap space and finds a candidate page to be swapped out of the main memory and into the swap space based on (FIFO, NFU, SCFIFO) flag set during `make`.

  - `printStats()` and `procDump()` system calls: `printStats()` prints the details of the current process, and `procDump()` prints all current processes. They are used in myMemTest.c to print the results and do away with `ctrl+P` during execution.
