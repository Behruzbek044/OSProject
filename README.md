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
## Data Structures:

**freepg**: 

  - This is a node of linked lists of pages (up to a maximum of 15) that are present in both the main memory and the swap space. It contains the virtual address of the page, its age, pointers to the next and previous nodes in the list, and the location of the page in the swap space.
  
**proc**:

  - This structure has been updated to include arrays of freepg for both main memory and swap space. It also includes additional metadata such as the number of swaps, the number of pages in main memory and swap space, and the number of page faults. 

## Modified Functions:

  - `allocproc(void)` and `exec()`: These functions are responsible for finding an empty process entry in the process table array and executing it, respectively. They have been modified to initialize all process metadata to 0 and all addresses to 0xffffffff. If the NONE flag is not defined, `exec()` creates a swap file for every process except for init and sh.

  - `allocuvm()`: This function is responsible for allocating memory for the process. It has been modified to track if the number of pages in the physical memory exceeds 15. If it does, it calls the appropriate paging scheme through the `writePagesToSwapFile()` function, and then updates the number of pages in the process's metadata using the `recordNewPage()` function.

  - `deallocuvm()`: This function deallocates memory from the physical memory and frees both the `free_pages` and `swap_file_pages` arrays of the process it is called for. It also decreases the counts of pages in both main memory and swap space.

  - `fork()`: This system call has been modified to copy the metadata of a process, including `swap_space_pages` and `free_pages` arrays, and the number of pages in swap file and main memory, to the child process. However, the number of page faults and page swaps are not copied to the child.

  - `exit()`: This system call has been modified to delete the metadata associated with the process. It closes the swap file and removes the pointers, and sets all the number entries to 0.

  - `procdump()`: This function has been modified with the `custom_proc_print()` function to print the counts of pages in swap space and in main memory, page faults, number of swaps, and the percentage of free pages in the physical memory.

  - `trap(struct trapframe *tf)`: This function has been modified to call the `updateNFUState()` function in case of timer interrupts to increase the "age" of pages when the NFU paging scheme is being used. It has also been modified to handle a page fault by defining T_PGFLT for trap 14.

## New Functions:

  - `WritePagesToSwapFile(char *va)`: This function calls the appropriate paging scheme (FIFO, NFU, SCFIFO) depending on the flag that was set during make. It writes the incoming page into the physical memory and selects a candidate page to be removed from the main memory and written to the swap space.

  - `recoredNewPage(char *va)`: This function writes the metadata of the currently added page into the corresponding entry in the `free_pages` array of the process. It also increases the count of pages in the physical memory.

  - `swapPages(char *va)`: This function retrieves the page with the given virtual address "va" from the swap space and finds a candidate page to be swapped out of the main memory and into the swap space based on the paging scheme (FIFO, NFU, SCFIFO) defined during make.

  - `printStats()` and `procDump()` system calls: These functions print the details of the current process and all current processes, respectively. They are used in `myMemTest.c` to print the results and eliminate the need for `ctrl+P` during execution.
