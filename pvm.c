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
void memused(int pid);
void mapva(int pid, uint64_t va);    
void pte(int pid, uint64_t va);
void maprange(int pid, uint64_t va1, uint64_t va2);
void mapall(int pid);
void mapallin(int pid);
void alltablesize(int pid);


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

    printf("Information for frame %lu:\n", pfn);
    printf("Mapping count: %lu\n", pagecount);
    printf("Flags: %lu\n", pageflags);
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
    uint64_t total_virt         = 0;
    uint64_t total_phys_excl    = 0;
    uint64_t total_phys         = 0;

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

    printf("Total virtual memory used: %lu KB\n", total_virt / 1024);
    printf("Total physical memory used (exclusive): %lu KB\n", total_phys_excl * PAGESIZE / 1024);
    printf("Total physical memory used (shared + exclusive): %lu KB\n", total_phys * PAGESIZE / 1024);

}

void mapva(int pid, uint64_t va)
{

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

    printf("Pagemap entry for VA 0x%lx:\n", va);
    printf("Page frame number: 0x%lx\n", get_entry_frame(pagemap_entry));
    printf("Present: %d\n", (pagemap_entry & (1ULL << 63)) != 0);
    printf("Swapped: %d\n", (pagemap_entry & (1ULL << 62)) != 0);
    printf("File-page or shared-anon: %d\n", (pagemap_entry & (1ULL << 61)) != 0);
    printf("Page exclusively mapped: %d\n", (pagemap_entry & (1ULL << 56)) != 0);
    printf("Soft-dirty: %d\n", (pagemap_entry & (1ULL << 55)) != 0);

    if (pagemap_entry & (1ULL << 62)) 
    {   // page is swapped
        uint64_t swap_offset = (pagemap_entry >> 5) & 0x3FFFFFFFFFF;
        uint64_t swap_type = pagemap_entry & 0x1F;
        printf("Swap offset: 0x%lx\n", swap_offset);
        printf("Swap type: 0x%lx\n", swap_type);
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
    while (fscanf(maps, "%lx-%lx", &start, &end) != EOF) 
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
            printf("VA 0x%lx: unused\n", va);
            continue;
        }

        uint64_t pagemap_entry;
        uint64_t virt_page_num = va / PAGESIZE;
        lseek(pagemap, virt_page_num * PAGEMAP_ENTRY_SIZE, SEEK_SET);
        if (read(pagemap, &pagemap_entry, PAGEMAP_LENGTH) != PAGEMAP_LENGTH) 
        {
            //printf("Failed to read pagemap entry for VA 0x%llx\n", char* pid, char* vava);  //TO BE FIXED!!!!!!!!!
            continue;
        }

        if ((pagemap_entry & (1ULL << 63)) == 0) 
        {
            printf("VA 0x%lx: not-in-memory\n", va);
        } 
        else
        {
            printf("VA 0x%lx: Frame 0x%lx\n", va, get_entry_frame(pagemap_entry));
        }
    }

    close(pagemap);
}

void mapall(int pid) {
    char pagemap_file[64];
    sprintf(pagemap_file, "/proc/%d/pagemap", pid);

    int pagemap = open(pagemap_file, O_RDONLY);
    if (pagemap < 0) {
        printf("Failed to open pagemap file\n");
        return;
    }

    char maps_file[64];
    sprintf(maps_file, "/proc/%d/maps", pid);

    FILE* maps_fp = fopen(maps_file, "r");
    if (maps_fp == NULL) {
        printf("Failed to open maps file\n");
        close(pagemap);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), maps_fp) != NULL) {
        uint64_t va_start, va_end;
        if (sscanf(line, "%lx-%lx", &va_start, &va_end) != 2) {
            continue;
        }

        for (uint64_t va = va_start; va < va_end; va += PAGESIZE) {
            uint64_t pagemap_entry;
            uint64_t virt_page_num = va / PAGESIZE;
            lseek(pagemap, virt_page_num * PAGEMAP_ENTRY_SIZE, SEEK_SET);
            if (read(pagemap, &pagemap_entry, PAGEMAP_LENGTH) != PAGEMAP_LENGTH) {
                printf("Failed to read pagemap entry for VA 0x%lx\n", va);
                continue;
            }

            if ((pagemap_entry & (1ULL << 63)) == 0) {
                printf("VA 0x%lx: not-in-memory\n", va);
            } else {
                printf("VA 0x%lx: Frame 0x%lx\n", va, get_entry_frame(pagemap_entry));
            }
        }
    }

    fclose(maps_fp);
    close(pagemap);
}

void mapallin(int pid) {
    char pagemap_file[64];
    sprintf(pagemap_file, "/proc/%d/pagemap", pid);

    int pagemap = open(pagemap_file, O_RDONLY);
    if (pagemap < 0) {
        printf("Failed to open pagemap file\n");
        return;
    }

    char maps_file[64];
    sprintf(maps_file, "/proc/%d/maps", pid);

    FILE* maps_fp = fopen(maps_file, "r");
    if (maps_fp == NULL) {
        printf("Failed to open maps file\n");
        close(pagemap);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), maps_fp) != NULL) {
        uint64_t va_start, va_end;
        if (sscanf(line, "%lx-%lx", &va_start, &va_end) != 2) {
            continue;
        }

        for (uint64_t va = va_start; va < va_end; va += PAGESIZE) {
            uint64_t pagemap_entry;
            uint64_t virt_page_num = va / PAGESIZE;
            lseek(pagemap, virt_page_num * PAGEMAP_ENTRY_SIZE, SEEK_SET);
            if (read(pagemap, &pagemap_entry, PAGEMAP_LENGTH) != PAGEMAP_LENGTH) {
                printf("Failed to read pagemap entry for VA 0x%lx\n", va);
                continue;
            }

            if ((pagemap_entry & (1ULL << 63)) != 0) {
                printf("VA 0x%lx: Frame 0x%lx\n", va, get_entry_frame(pagemap_entry));
            }
        }
    }

    fclose(maps_fp);
    close(pagemap);
}

void alltablesize(int pid)
{
    int         level_of_paging = 4;
    int         paging_levels[4] = {0};
    char        pagemap_file[64];
    char        pagemap_line[256];
    FILE*         pagemap;
    uint64_t    start;
    uint64_t    end;
    uint64_t    page_table_size = 0;
    char        dummy_permissions[5];
    sprintf(pagemap_file, "/proc/%d/maps", pid);

    pagemap =   fopen(pagemap_file, "r");
    if (pagemap == NULL)
    {
        printf("Failed to open pagemap file\n");
        return;
    }

    ssize_t bytes_read;
    // Read the file line by line
    //printf("haha\n");
    
    while (fgets(pagemap_line, sizeof(pagemap_line), pagemap) != NULL) {
        pagemap_line[strcspn(pagemap_line, "\n")] = '\0';  // Null-terminate the line
        sscanf(pagemap_line, "%lx-%lx %s %*x %*x:%*x %*d", (unsigned long*)&start, (unsigned long*)&end, dummy_permissions);

        uint64_t mappingSize = end - start;
        uint64_t alignedSize = (mappingSize + PAGESIZE - 1) & ~(PAGESIZE - 1);
        uint64_t mappingPageTableSize = alignedSize / sizeof(uint64_t);

        page_table_size += mappingPageTableSize;

        for (int i = 0; i < level_of_paging; i++) {
            paging_levels[i] += (mappingPageTableSize >> (9 * (3 - i))) & 0x1FF;
        }
    }

    fclose(pagemap);

    // Calculate the size in kilobytes
    uint64_t pageTableSizeKB = page_table_size * sizeof(uint64_t) / 1024;

    // Print the total page table size
    printf("(pid=%d) total memory occupied by 4-level page table: %lu KB (%lu frames)\n",
           pid, pageTableSizeKB, page_table_size);

    // Print the number of page tables at each level
    printf("(pid=%d) number of page tables used: level1=%d, level2=%d, level3=%d, level4=%d\n",
           pid, paging_levels[0], paging_levels[1], paging_levels[2], paging_levels[3]);

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
        mapva(atoi(argv[2]), atoi(argv[3]));
    } 
    else if (!strcmp(command, "-pte")) 
    {
        pte(atoi(argv[2]), strtoull(argv[3], NULL, 10));
    } 
    else if (!strcmp(command, "-maprange")) 
    {
        maprange(atoi(argv[2]), strtoull(argv[3], NULL, 10), strtoull(argv[4], NULL, 10));
    } 
    else if (!strcmp(command, "-mapall")) 
    {
        mapall(atoi(argv[2]));
    } 
    else if (!strcmp(command, "-mapallin")) 
    {
        mapallin(atoi(argv[2]));
    } 
    else if (!strcmp(command, "-alltablesize")) 
    {
        alltablesize(atoi(argv[2]));
    } 
    else 
    {
        printf("Invalid command\n");
        return -1;
    }

    return 0;
}
