# CS342-Project4
In this project you will implement a Linux program, in C, that will give
memory related information for a process and for the physical memory of the
system. The program will be called pvm. It will take the memory related
information from the /proc virtual file system, which is an interface to the
kernel for user-space programs (applications). In the /proc directory, there
are a lot of files that can be read by a user program or viewed by a user to get
various information about the system state and processes. The related
information is retrieved from various kernel data structures and variables
(from kernel space). Hence, the content of these files are derived from kernel
memory, not from disk. In /proc directory, there is a sub-directory for each
process, to get process specific information maintained by the kernel.
The project will be done on a 64 bit machine with x86-64 architecture. The
x86-64 architecture is using 4-level page tables for a process. Your program
will use the following four /proc files to retrieve the requested information.
