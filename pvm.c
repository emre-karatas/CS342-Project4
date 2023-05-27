#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#define KPAGECOUNT_PATH "/proc/kpagecount"
#define KPAGEFLAGS_PATH "/proc/kpageflags"

#define PAGEMAP_LENGTH 8
#define PAGESIZE 4096
#define PAGEMAP_ENTRY_SIZE 8


// Function prototypes
void frameinfo(char* pfn);
void memused(int pid);                        //Prototype and declaration are different
void mapva(char* pid, char* va);    
void pte(int pid, uint64_t va);                  //Prototype and declaration are different
void maprange(int pid, uint64_t va1, uint64_t va2); //Prototype and declaration are different
void mapall(char* pid);
void mapallin(char* pid);
void alltablesize(char* pid);


uint64_t get_entry_frame(uint64_t entry) 
{
    return entry & 0x7FFFFFFFFFFFFF;
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



void memused(int pid) 
{
    char pagemap_file[64];
    char maps_file[64];
    sprintf(pagemap_file, "/proc/%dint pid, uint64_t va1, uint64_t va2/pagemap", pid);
    sprintf(maps_file, "/proc/%d/maps", pid);

    FILE* maps = fopen(maps_file, "r");
    if (maps == NULL) {
        printf("Failed to open maps file\n");
        return;
    }

    char line[256];
    uint64_t total_virt = 0, total_phys_excl = 0, total_phys = 0;

    while (fgets(line, sizeof(line), maps)) 
    {
        uint64_t virt_start, virt_end;
        sscanf(line, "%lx-%lx", &virt_start, &virt_end);

        uint64_t virt_size = virt_end - virt_start;
        total_virt += virt_size;

        int pagemap = open(pagemap_file, O_RDONLY);
        if (pagemap < 0) 
        {
            printf("Failed to open pagemap file\n");
            fclose(maps);
            return;
        }

        uint64_t num_pages = virt_size / PAGESIZE;
        for (uint64_t i = 0; i < num_pages; i++) 
        {
            uint64_t virt_addr = virt_start + i * PAGESIZE;
            uint64_t offset = virt_addr / PAGESIZE * PAGEMAP_ENTRY_SIZE;

            uint64_t entry;
            lseek(pagemap, offset, SEEK_SET);
            if (read(pagemap, &entry, PAGEMAP_LENGTH) != PAGEMAP_LENGTH) 
            {
                printf("Failed to read pagemap entry\n");
                close(pagemap);
                fclose(maps);
                return;
            }
            if (entry & (1ULL << 63)) 
            { // page is present in memory
                total_phys++;
                if (entry & (1ULL << 62)) 
                { // page is exclusive
                    total_phys_excl++;
                }
            }
        }

        close(pagemap);
    }

    fclose(maps);

    printf("Total virtual memory used: %llu KB\n", total_virt / 1024);
    printf("Total physical memory used (exclusive): %llu KB\n", total_phys_excl * PAGESIZE / 1024);
    printf("Total physical memory used (shared + exclusive): %llu KB\n", total_phys * PAGESIZE / 1024);

}

void pte(int pid, uint64_t va) 
{
    char pagemap_file[64];
    sprintf(pagemap_file, "/proc/%d/pagemap", pid);

    int pagemap = open(pagemap_file, O_RDONLY);
    if (pagemap < 0) 
    {
        printf("Failed to open pagemap file\n");
        return;
    }

    uint64_t virt_page_num = va / PAGESIZE;

    uint64_t pagemap_entry;
    lseek(pagemap, virt_page_num * PAGEMAP_ENTRY_SIZE, SEEK_SET);
    if (read(pagemap, &pagemap_entry, PAGEMAP_LENGTH) != PAGEMAP_LENGTH) 
    {
        printf("Failed to read pagemap entry\n");
        close(pagemap);
        return;
    }

    close(pagemap);

    printf("Pagemap entry for VA 0x%llx:\n", va);
    printf("Page frame number: 0x%llx\n", get_entry_frame(pagemap_entry));
    printf("Present: %d\n", (pagemap_entry & (1ULL << 63)) != 0);
    printf("Swapped: %d\n", (pagemap_entry & (1ULL << 62)) != 0);
    printf("File-page or shared-anon: %d\n", (pagemap_entry & (1ULL << 61)) != 0);
    printf("Page exclusively mapped: %d\n", (pagemap_entry & (1ULL << 56)) != 0);
    printf("Soft-dirty: %d\n", (pagemap_entry & (1ULL << 55)) != 0);

    if (pagemap_entry & (1ULL << 62)) 
    {   // page is swapped
        uint64_t swap_offset = (pagemap_entry >> 5) & 0x3FFFFFFFFFF;
        uint64_t swap_type = pagemap_entry & 0x1F;
        printf("Swap offset: 0x%llx\n", swap_offset);
        printf("Swap type: 0x%llx\n", swap_type);
    }
}

bool is_va_used(uint64_t va, char *maps_file) 
{
    FILE *maps = fopen(maps_file, "r");
    if (maps == NULL) 
    {
        printf("Failed to open maps file\n");
        return false;
    }

    uint64_t start, end;
    while (fscanf(maps, "%llx-%llx", &start, &end) != EOF) 
    {
        if (va >= start && va < end) 
        {
            fclose(maps);
            return true;
        }
    }

    fclose(maps);
    return false;
}

void maprange(int pid, uint64_t va1, uint64_t va2) 
{
    char pagemap_file[64];
    sprintf(pagemap_file, "/proc/%d/pagemap", pid);

    int pagemap = open(pagemap_file, O_RDONLY);
    if (pagemap < 0) 
    {
        printf("Failed to open pagemap file\n");
        return;
    }

    char maps_file[64];
    sprintf(maps_file, "/proc/%d/maps", pid);

    for (uint64_t va = va1; va < va2; va += PAGESIZE) 
    {
        if (!is_va_used(va, maps_file)) 
        {
            printf("VA 0x%llx: unused\n", va);
            continue;
        }

        uint64_t pagemap_entry;
        uint64_t virt_page_num = va / PAGESIZE;
        lseek(pagemap, virt_page_num * PAGEMAP_ENTRY_SIZE, SEEK_SET);
        if (read(pagemap, &pagemap_entry, PAGEMAP_LENGTH) != PAGEMAP_LENGTH) 
        {
            printf("Failed to read pagemap entry for VA 0x%llx\n", char* pid, char* vava);  //TO BE FIXED!!!!!!!!!
            continue;
        }

        if ((pagemap_entry & (1ULL << 63)) == 0) 
        {
            printf("VA 0x%llx: not-in-memory\n", va);
        } 
        else 
        {
            printf("VA 0x%llx: Frame 0x%llx\n", va, get_entry_frame(pagemap_entry));
        }
    }

    close(pagemap);
}



int main(int argc, char* argv[]) 
{
    if (argc < 3) 
    {
        printf("Please provide valid arguments\n");
        return -1;
    }

    char* command = argv[1];
    if (!strcmp(command, "-frameinfo")) 
    {
        frameinfo(argv[2]);
    } 
    else if (!strcmp(command, "-memused")) 
    {
        memused(atoi(argv[2]));
    } 
    else if (!strcmp(command, "-mapva")) 
    {
        mapva(argv[2], argv[3]);
    } 
    else if (!strcmp(command, "-pte")) 
    {
        pte(atoi(argv[2]), argv[3]);
    } 
    else if (!strcmp(command, "-maprange")) 
    {
        maprange(argv[2], strtoull(argv[3], NULL, 10), strtoull(argv[4], NULL, 10));
    } 
    else if (!strcmp(command, "-mapall")) 
    {
        mapall(argv[2]);
    } 
    else if (!strcmp(command, "-mapallin")) 
    {
        mapallin(argv[2]);
    } 
    else if (!strcmp(command, "-alltablesize")) 
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

