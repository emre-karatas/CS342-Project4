#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#define PAGESIZE 4096
#define KPAGECOUNT_PATH "/proc/kpagecount"
#define KPAGEFLAGS_PATH "/proc/kpageflags"

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

uint64_t is_free_frame(uint64_t pfn) 
{
    FILE* kpagecount_file = fopen(KPAGECOUNT_PATH, "rb");
    FILE* kpageflags_file = fopen(KPAGEFLAGS_PATH, "rb");

    if (!kpagecount_file || !kpageflags_file) 
    {
        printf("Could not open one of the proc files\n");
        return 0;
    }

    uint64_t pagecount;
    uint64_t pageflags;

    lseek(kpagecount_file, pfn * sizeof(uint64_t), SEEK_SET);
    fread(&pagecount, sizeof(uint64_t), 1, kpagecount_file);

    lseek(kpageflags_file, pfn * sizeof(uint64_t), SEEK_SET);
    fread(&pageflags, sizeof(uint64_t), 1, kpageflags_file);

    fclose(kpagecount_file);
    fclose(kpageflags_file);

    return pagecount == 0 || (pageflags & (1ULL << 60)) != 0;  // NOPAGE flag is assumed to be the 60th bit
}

void freefc(char* pfn1, char* pfn2) 
{
    uint64_t start = strtoull(pfn1, NULL, 16);
    uint64_t end = strtoull(pfn2, NULL, 16);

    uint64_t freeFrames = 0;

    for (uint64_t pfn = start; pfn < end; ++pfn) {
        if (is_free_frame(pfn)) {
            ++freeFrames;
        }
    }

    printf("Number of free frames: %llu\n", freeFrames);
}

uint64_t get_frame_flags(uint64_t pfn) 
{
    int kpageflags_fd = open(KPAGEFLAGS_PATH, O_RDONLY);
    if (kpageflags_fd == -1) 
    {
        printf("Could not open kpageflags file\n");
        return 0;
    }

    uint64_t pageflags;

    lseek(kpageflags_fd, pfn * sizeof(uint64_t), SEEK_SET);
    read(kpageflags_fd, &pageflags, sizeof(uint64_t));

    close(kpageflags_fd);

    return pageflags;
}

uint64_t get_mapping_count(uint64_t pfn) 
{
    int kpagecount_fd = open(KPAGECOUNT_PATH, O_RDONLY);
    if (kpagecount_fd == -1) 
    {
        printf("Could not open kpagecount file\n");
        return 0;
    }

    uint64_t pagecount;

    lseek(kpagecount_fd, pfn * sizeof(uint64_t), SEEK_SET);
    read(kpagecount_fd, &pagecount, sizeof(uint64_t));

    close(kpagecount_fd);

    return pagecount;
}

void frameinfo(char* pfn_str) 
{
    uint64_t pfn = strtoull(pfn_str, NULL, 16);

    uint64_t pageflags = get_frame_flags(pfn);
    uint64_t pagecount = get_mapping_count(pfn);

    printf("Information for frame %llu:\n", pfn);
    printf("Mapping count: %llu\n", pagecount);
    printf("Flags: %llu\n", pageflags);
}



void memused(char* pid) 
{
    // The path to the process's memory map in the /proc filesystem.
    char path[256];
    sprintf(path, "/proc/%s/maps", pid);

    // Open the memory map.
    FILE* file = fopen(path, "r");
    if (!file) 
    {
        printf("Could not open %s\n", path);
        return;
    }

    unsigned long long totalSize = 0;

    // Read each line of the memory map.
    char line[256];
    while (fgets(line, sizeof(line), file)) 
    {
        unsigned long long start, end;
        if (sscanf(line, "%llx-%llx", &start, &end) == 2) 
        {
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

