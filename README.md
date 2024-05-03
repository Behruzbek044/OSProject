# Paging Implementation in Xv6

Our project utilizes the Xv6 OS kernel implementation provided by <https://github.com/mit-pdos/xv6-riscv>, to whom we extend our gratitude. This project enhances the memory management unit of the Xv6 kernel through the introduction of demand paging support. We have developed and evaluated a variety of page replacement strategies.

## Implementation Details
### Running the code:
```shell
sudo apt install qemu qemu-system-x86
sudo apt install libc6:i386
git clone https://github.com/Behruzbek044/OSProject xv6
sudo chmod 700 -R xv6
cd xv6
make clean
make qemu SELECTION=FLAG1 VERBOSE_PRINT=FLAG2
```
### Structs:
**freepg**: Node of linked lists of pages (max = 15) present in the main memory and the swap space.
- <img src="https://i.imgur.com/p5Vd1Ck.png"/>

**proc:**
- Added arrays of freepg for both main memory and physical memory.
- Added other metadata like # of swaps, # of pages in main memory and swap space, # page faults.
- <img src="https://i.imgur.com/L50HWfO.png" style="width:400px;"/>
