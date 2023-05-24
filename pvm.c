#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function prototypes
void freefc(char* pfn1, char* pfn2);
void frameinfo(char* pfn);
void memused(char* pid);
void mapva(char* pid, char* va);
void pte(char* pid, char* va);
void maprange(char* pid, char* va1, char* va2);
void mapall(char* pid);
void mapallin(char* pid);
void alltablesize(char* pid);

void memused(char* pid) 
{
    // The path to the process's memory map in the /proc filesystem.
    char path[256];
    sprintf(path, "/proc/%s/maps", pid);

    // Open the memory map.
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("Could not open %s\n", path);
        return;
    }

    unsigned long long totalSize = 0;

    // Read each line of the memory map.
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        unsigned long long start, end;
        if (sscanf(line, "%llx-%llx", &start, &end) == 2) {
            totalSize += end - start;
        }
    }

    fclose(file);

    // Convert the size from bytes to kilobytes.
    totalSize /= 1024;
    printf("Total virtual memory used by process %s: %llu KB\n", pid, totalSize);

    // TODO: Calculate the physical memory used by the process.
}


int main(int argc, char* argv[]) 
{
    if (argc < 3) 
    {
        printf("Please provide valid arguments\n");
        return -1;
    }

    char* command = argv[1];
    if (strcmp(command, "-freefc") == 0) 
    {
        freefc(argv[2], argv[3]);
    } 
    else if (strcmp(command, "-frameinfo") == 0) 
    {
        frameinfo(argv[2]);
    } 
    else if (strcmp(command, "-memused") == 0) 
    {
        memused(argv[2]);
    } 
    else if (strcmp(command, "-mapva") == 0) 
    {
        mapva(argv[2], argv[3]);
    } 
    else if (strcmp(command, "-pte") == 0) 
    {
        pte(argv[2], argv[3]);
    } 
    else if (strcmp(command, "-maprange") == 0) 
    {
        maprange(argv[2], argv[3], argv[4]);
    } 
    else if (strcmp(command, "-mapall") == 0) 
    {
        mapall(argv[2]);
    } 
    else if (strcmp(command, "-mapallin") == 0) 
    {
        mapallin(argv[2]);
    } 
    else if (strcmp(command, "-alltablesize") == 0) 
    {
        alltablesize(argv[2]);
    } 
    else 
    {
        printf("Invalid command\n");
        return -1;
    }

    return 0;
}

