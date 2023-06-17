# Project 4: Process Virtual Memory (pvm) in Linux

## Introduction 

In this project, you will create a robust Linux program, written in C, called `pvm` (Process Virtual Memory). This program will present users with valuable insights about memory-related information for a given process, and also the physical memory of the system. The beauty of this program lies in its ability to fetch information from the `/proc` virtual file system, an interface that helps user-space programs interact with the kernel. 

The `/proc` directory is essentially a window into the kernel's world. It contains numerous files that, when accessed, can provide diverse information about the system state and processes. This treasure trove of information is fetched from various kernel data structures and variables (from kernel space). It's important to remember that the content of these files is derived from kernel memory and not from disk.

Each process in the system has its own sub-directory in the `/proc` directory, making it possible to fetch process-specific information maintained by the kernel.

This project will be carried out on a 64-bit machine featuring an x86-64 architecture. The x86-64 architecture uses a 4-level page table for each process. Your `pvm` program will utilize the following four `/proc` files to fetch the requested information:

1. **/proc/pid/maps**: This text file provides information about the virtual memory areas of a process (pid). By reading this file, the program can discern which parts of the virtual memory of a process are in use and which parts are not.
   
2. **/proc/pid/pagemap**: This binary file allows the program to get information about the virtual pages of a process. It contains one entry (64 bits) per page, storing the respective frame number if the page is in memory.
   
3. **/proc/kpagecount**: This binary file enables the program to determine the number of times a page in a frame is mapped, i.e., how many processes are using the page at that moment.
   
4. **/proc/kpageflags**: Another binary file, this lets the program extract additional information about each physical frame and the page contained within it, including certain flags associated with each frame/page.

You can refer to the kernel documentation for further details about these files. 

## Program Design and Implementation

Your `pvm` program should be designed to efficiently read from these binary and text files using the `open()`, `read()`, `close()` system calls for binary files, and the `fopen()`, `fscanf()`, `fclose()` functions for text files. Additionally, the `lseek()` call is essential for jumping to an entry and reading it. 

The program will be restricted to use only these files and no other files from the `/proc` directory.

One feature of this program is its ability to display the virtual memory layout of a process, similar to what you get by typing `cat /proc/pid/maps` in the terminal. This will provide a listing of used virtual memory areas (regions) of the process.

**Note:** You will find the sample output in the project documentation.

## Program Invocation and Options

The `pvm` program supports a number of command-line options, allowing users to fetch detailed memory-related information. Here are the possible invocations:

1. **pvm -frameinfo PFN**: Prints detailed information (various flag values and mapping count) for the specified frame.

2. **pvm -memused PID**: Finds out the total amount of virtual memory and physical memory used by the process PID (in KB).

3. **pvm -mapva PID VA**: Finds and prints out the physical address corresponding to the virtual address VA for the process PID.



4. **pvm -pte PID VA**: Finds and prints out detailed information (including the frame number and various flags) for the page corresponding to the virtual address VA of the process PID.

5. **pvm -maprange PID VA1 VA2**: Finds and prints out mappings for the virtual addresses in the range VA1 to VA2 for the process PID.

6. **pvm -mapall PID**: Finds and prints out mappings for all the virtual memory areas of the process PID.

7. **pvm -mapallin PID**: Similar to `-mapall`, but only prints information about used pages that are in memory.

8. **pvm -alltablesize PID**: Calculates the total memory required to store page table information for the process PID.

## Invocation Example

Here is how the program can be invoked:

```
sudo ./pvm <option> <arguments>
```

For instance, to find and print mappings for all the virtual memory areas of a process with PID 1234, you would run:

```
sudo ./pvm -mapall 1234
```

Remember, you need superuser permissions to fetch details about a process not owned by you. So, be sure to use `sudo` when invoking the program for a non-owned process.

By implementing this program, you will get a profound understanding of memory management in Linux, the working of page tables, and process memory layout. Good luck with your coding journey!
