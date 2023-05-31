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
#define ENTRY_PER_PAGE 512

// Function prototypes
void frameinfo(uint64_t pfn);
void memused(int pid);
void mapva(int pid, uint64_t va);    
void pte(int pid, uint64_t va);
void maprange(int pid, uint64_t va1, uint64_t va2);
void mapall(int pid);
void mapallin(int pid);
void alltablesize(int pid);

uint64_t pfn_va_formatter(char* arg);

uint64_t get_entry_frame(uint64_t entry) {
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

void frameinfo(uint64_t pfn) 
{    
    const char* flag_names[] = {
        "LOCKED", "ERROR", "REFERENCED", "UPTODATE", "DIRTY", "LRU", "ACTIVE", "SLAB",
        "WRITEBACK", "RECLAIM", "BUDDY", "MMAP", "ANON", "SWAPCACHE", "SWAPBACKED",
        "COMPOUND_HEAD", "COMPOUND_TAIL", "HUGE", "UNEVICTABLE", "HWPOISON",
        "NOPAGE", "KSM", "THP", "BALLOON", "ZERO_PAGE", "IDLE"
    };

    uint64_t flags = get_frame_flags(pfn);
    int num_flags = sizeof(flag_names) / sizeof(flag_names[0]);
    int five_cnt = 0;
    for (int i = 0; i < num_flags; i++) 
    {
        printf("%02d. %-20s",i,flag_names[i]);
        five_cnt++;
        if(five_cnt == 5) {
            printf("\n");
            five_cnt = 0;
        }
    }
    printf("\n");
    
    printf("FRAME#\t\t");
    for (int i = 0; i < num_flags; i++) 
    {
        printf("%02d ", i);
    }
    printf("\n");

    printf("0x%012lx\t", pfn);
    for (int i = 0; i < num_flags; i++) 
    {
        printf(" %lu ", (flags >> i) & 1);
    }
    printf("\n");
}

void memused(int pid) 
{
    char line[512];

    // Open the file for reading
    char mapfile_path[256];
    sprintf(mapfile_path,"/proc/%d/maps", pid);
    FILE *mapfile= fopen(mapfile_path, "r");

    if (!mapfile)
    {
        perror("Unable to open map file");
        return;
    }

    uint64_t totalVM = 0;
    uint64_t totalPM = 0;
    uint64_t exclusivePM = 0;

    // Read the file line by line until EOF is reached
    while (fgets(line, sizeof(line), mapfile) != NULL) 
    {
        char *token;
        char *firstPart = NULL;
        char *secondPart = NULL;

        token = strtok(line, "-");
        if (token != NULL) 
        {
            firstPart = strdup(token);   // Using strdup simplifies the process
        }

        token = strtok(NULL, " ");
        if (token != NULL) 
        {
            secondPart = strdup(token);
        }

        if (firstPart != NULL && secondPart != NULL)
        {
            uint64_t startAddr = strtoul(firstPart, NULL, 16);
            uint64_t endAddr = strtoul(secondPart, NULL, 16);

            char pagemap_path[256];
            sprintf(pagemap_path, "/proc/%d/pagemap", pid);

            int pagemapfile = open(pagemap_path, O_RDONLY);
            if (pagemapfile < 0)
            {
                perror("Failed to open pagemap file");
                free(firstPart);
                free(secondPart);
                continue;
            }

            for (uint64_t i = startAddr; i < endAddr; i += PAGESIZE) 
            {
              // find the frame number
              uint64_t offset = i / PAGESIZE * sizeof(uint64_t);
              lseek(pagemapfile, offset, SEEK_SET);
              uint64_t entry;
              read(pagemapfile, &entry, sizeof(uint64_t));
              uint64_t valid = (entry >> 63) & 1;
              if (valid) 
              {
                uint64_t count;
                uint64_t frame_number = entry & ((1UL << 55) - 1);
                uint64_t offset2 = frame_number * sizeof(uint64_t);
                int kpagecount = open("/proc/kpagecount" , O_RDONLY);
                if (kpagecount < 0)
                {
                    perror("Failed to open kpagecount file");
                    continue;
                }
                lseek(kpagecount, offset2, SEEK_SET);
                read(kpagecount, &count, sizeof(uint64_t));

                if (count == 1) 
                {
                    exclusivePM += PAGESIZE;
                }
                if (count >= 1) 
                {
                  totalPM += PAGESIZE;
                }
                close(kpagecount);
              }
              totalVM += PAGESIZE;
            }
            close(pagemapfile);
        }
        free(firstPart);
        free(secondPart);
    }
    totalPM += PAGESIZE;
    printf("(pid=%d) memused: virtual=%ld KB, pmem_all=%ld KB, pmem_alone=%ld KB, mappedonce=%ld KB\n",pid,totalVM/1024,totalPM/1024,exclusivePM/1024,exclusivePM/1024);

    // Close the file
    fclose(mapfile);
}


void mapva(int pid, uint64_t va) {
    char pagemap_file[64];
    FILE* pagemap;
    sprintf(pagemap_file, "/proc/%d/pagemap", pid);

    pagemap = fopen(pagemap_file, "r");
    if (pagemap == NULL) {
        printf("Failed to open pagemap file\n");
        return;
    }

    // Find the corresponding physical address
    uint64_t virt_page_num = va / PAGESIZE;
    fseek(pagemap, virt_page_num * PAGEMAP_ENTRY_SIZE, SEEK_SET);
    uint64_t pagemap_entry;
    if (fread(&pagemap_entry, PAGEMAP_LENGTH, 1, pagemap) != 1) {
        printf("Failed to read pagemap entry for VA 0x%lx\n", va);
        fclose(pagemap);
        return;
    }
    uint64_t physical_address = get_entry_frame(pagemap_entry) * PAGESIZE + (va % PAGESIZE);
    uint64_t fnum = get_entry_frame(pagemap_entry);

    fclose(pagemap);

    // Print the physical address and frame number in hexadecimal format
    printf("va=0x%012lx: physical_address=0x%016lx, fnum=0x%09lx\n", va, physical_address, fnum);
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

    printf("[vaddr=0x%012lx, vpn=0x%09lx]: present=%d, swapped=%d, file-anon=%d, exclusive=%d, softdirty=%d, number=0x%09lx\n",
            va,                                     //Pagemap entry for VA
            va >> 12,                               //Page frame number
            (pagemap_entry & (1ULL << 63)) != 0,    //Present
            (pagemap_entry & (1ULL << 62)) != 0,    //Swapped
            (pagemap_entry & (1ULL << 61)) != 0,    //File-page of shared-anon
            (pagemap_entry & (1ULL << 56)) != 0,    //Page exclusively mapped
            (pagemap_entry & (1ULL << 55)) != 0,    //Soft-dirty
            get_entry_frame(pagemap_entry));        //PFN
    /*
    printf("Pagemap entry for VA 0x%lx:\n", va);
    printf("Page frame number: 0x%lx\n", get_entry_frame(pagemap_entry));
    printf("Present: %d\n", (pagemap_entry & (1ULL << 63)) != 0);
    printf("Swapped: %d\n", (pagemap_entry & (1ULL << 62)) != 0);
    printf("File-page or shared-anon: %d\n", (pagemap_entry & (1ULL << 61)) != 0);
    printf("Page exclusively mapped: %d\n", (pagemap_entry & (1ULL << 56)) != 0);
    printf("Soft-dirty: %d\n", (pagemap_entry & (1ULL << 55)) != 0);
    */
    if (pagemap_entry & (1ULL << 62)) 
    {   // page is swapped
        uint64_t swap_offset = (pagemap_entry >> 5) & 0x3FFFFFFFFFF;
        uint64_t swap_type = pagemap_entry & 0x1F;
        printf("Swap offset: 0x%lx\n", swap_offset);
        printf("Swap type: 0x%lx\n", swap_type);
    }
}

int is_va_used(uint64_t va, const char* maps_file) {
    FILE* maps = fopen(maps_file, "r");
    if (maps == NULL) {
        printf("Failed to open maps file\n");
        return 0;
    }

    char line[512];
    while (fgets(line, sizeof(line), maps) != NULL) {
        uint64_t start, end;
        sscanf(line, "%lx-%lx", &start, &end);
        if (va >= start && va < end) {
            fclose(maps);
            return 1;
        }
    }

    fclose(maps);
    return 0;
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
            printf("va=0x%012lx: not-in-memory\n", va >> 12);
        } 
        else
        {
            printf("va=0x%012lx pfn=0x%09lx\n", va >> 12, get_entry_frame(pagemap_entry));
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
        char fname[256];  // Added to store the filename

        if (sscanf(line, "%lx-%lx %*s %*s %*s %*s %s", &va_start, &va_end, fname) != 3) {
            continue;
        }

        for (uint64_t va = va_start; va < va_end; va += PAGESIZE) {
            
            uint64_t pagemap_entry;
            uint64_t virt_page_num = va / PAGESIZE;
            lseek(pagemap, virt_page_num * PAGEMAP_ENTRY_SIZE, SEEK_SET);
            if (read(pagemap, &pagemap_entry, PAGEMAP_LENGTH) != PAGEMAP_LENGTH) {
                printf("Failed to read pagemap entry for VA 0x%09lx\n", va >> 12);
                continue;
            }

            if ((pagemap_entry & (1ULL << 63)) == 0) {
                FILE *status_fp;
                char status_file[64];
                sprintf(status_file, "/proc/%d/status", pid);
                status_fp = fopen(status_file, "r");
                char status_line[256];
                char swpd[64];
                while (fgets(status_line, sizeof(status_line), status_fp) != NULL) {
                    if (sscanf(status_line, "VmSwap: %s", swpd) == 1) {
                        break;
                    }
                }
                fclose(status_fp);
                printf("mapping: vpn=0x%09lx not-in-memory, swpd=%s, fname=%s\n", va >> 12, swpd, fname);
            } else {
                printf("mapping: vpn=0x%09lx pfn=0x%09lx, fname=%s\n", va >> 12, get_entry_frame(pagemap_entry), fname);
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
                printf("Failed to read pagemap entry for VA 0x%09lx\n", va >> 12);
                continue;
            }

            if ((pagemap_entry & (1ULL << 63)) != 0) {
                printf("mapping: vpn=0x%09lx: pfn=0x%09lx\n", va >> 12, get_entry_frame(pagemap_entry));
            }
        }
    }

    fclose(maps_fp);
    close(pagemap);
}

void alltablesize(int pid) {
    char path[64];
    sprintf(path, "/proc/%d/maps", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) 
    {
        perror("Could not open file");
        return;
    }

    char ****page_table = calloc(ENTRY_PER_PAGE, sizeof(char***));
    uint64_t paging_levels[4] = {0, 0, 0, 0};

    char line[256];
    while (fgets(line, sizeof(line), file)) 
    {
        uint64_t start, end;
        sscanf(line, "%lx-%lx", &start, &end);
        for(uint64_t addr=start; addr<end; addr+=PAGESIZE) 
        {
            uint64_t indices[4] = 
            {
                (addr >> 39) & 0x1FF,
                (addr >> 30) & 0x1FF,
                (addr >> 21) & 0x1FF,
                (addr >> 12) & 0x1FF
            };
            char ****pt1 = page_table;
            for(int i=0; i<4; i++) 
            {
                if(!*pt1) 
                {
                    *pt1 = calloc(ENTRY_PER_PAGE, sizeof(void*));
                    paging_levels[i]++;
                }
                pt1 = (char****) &(*pt1)[indices[i]];
            }
        }
    }
    fclose(file);
    // Free the dynamically allocated memory
    for (uint64_t i = 0; i < ENTRY_PER_PAGE; i++) {
        if (page_table[i]) {
            for (uint64_t j = 0; j < ENTRY_PER_PAGE; j++) {
                if (page_table[i][j]) {
                    for (uint64_t k = 0; k < ENTRY_PER_PAGE; k++) {
                        if (page_table[i][j][k]) {
                            free(page_table[i][j][k]);
                        }
                    }
                    free(page_table[i][j]);
                }
            }
            free(page_table[i]);
        }
    }
    free(page_table);
    uint64_t pageTableSizeKB = (paging_levels[0] + paging_levels[1] + paging_levels[2] + paging_levels[3]) * PAGEMAP_ENTRY_SIZE * ENTRY_PER_PAGE / 1024;
    printf("(pid=%d) total memory occupied by 4-level page table: %lu KB (%lu frames)\n",
           pid, pageTableSizeKB, paging_levels[0] + paging_levels[1] + paging_levels[2] + paging_levels[3]);

    printf("(pid=%d) number of page tables used: level1=%lu, level2=%lu, level3=%lu, level4=%lu\n",
           pid, paging_levels[0], paging_levels[1], paging_levels[2], paging_levels[3]);
}

/*void alltablesize(int pid)
{
    int level_of_paging = 4;
    int paging_levels[4] = {1,0,0,0};
    char pagemap_file[64];
    char pagemap_line[256];
    FILE* pagemap;
    uint64_t start;
    uint64_t end;
    char dummy_permissions[5];
    uint64_t num_page_tables = 1;

    int* level_2_prev_checking;
    int** level_3_prev_checking;
    int*** level_4_prev_checking;

    sprintf(pagemap_file, "/proc/%d/maps", pid);

    pagemap = fopen(pagemap_file, "r");
    if (pagemap == NULL)
    {
        printf("Failed to open pagemap file\n");
        return;
    }

    level_2_prev_checking = malloc(ENTRY_PER_PAGE * sizeof(int));
    for(int i = 0; i < ENTRY_PER_PAGE; ++i)
    {
        level_2_prev_checking[i] = 0;
    }
    level_3_prev_checking = malloc(ENTRY_PER_PAGE * sizeof(int*));
    for(int i = 0; i < ENTRY_PER_PAGE; ++i)
    {
        level_3_prev_checking[i] = malloc(ENTRY_PER_PAGE * sizeof(int));
        for(int j = 0; j < ENTRY_PER_PAGE; ++j)
        {
            level_3_prev_checking[i][j] = 0;
        }
    }
    level_4_prev_checking = malloc(ENTRY_PER_PAGE * sizeof(int**));
    for(int i = 0; i < ENTRY_PER_PAGE; ++i)
    {
        level_4_prev_checking[i] = malloc(ENTRY_PER_PAGE * sizeof(int*));
        for(int j = 0; j < ENTRY_PER_PAGE; ++j)
        {
            level_4_prev_checking[i][j] = malloc(ENTRY_PER_PAGE * sizeof(int));
            for(int k = 0; k < ENTRY_PER_PAGE; ++k)
            {
                level_4_prev_checking[i][j][k] = 0;
            }
        }
    }
    while (fgets(pagemap_line, sizeof(pagemap_line), pagemap) != NULL)
    {
        pagemap_line[strcspn(pagemap_line, "\n")] = '\0';  // Null-terminate the line
        sscanf(pagemap_line, "%lx-%lx %s %*x %*x:%*x %*d", &start, &end, dummy_permissions);

        uint64_t mappingSize = end - start;
        uint64_t numPageTableEntries = mappingSize >> 12;  // Shift by page offset (12 bits)
        //num_page_tables += numPageTableEntries / ENTRY_PER_PAGE;
        uint64_t level_indexes[4]; //0 for lvl1, 1 for lvl2, 2 for lvl3 3 for lvl4
        for (int i = 0; i < level_of_paging; i++)
        {
            level_indexes[i] = (start >> ( 12 + 9 * (level_of_paging - i) )) & 0x1FF;
            
            uint64_t pageTableEntries = (numPageTableEntries >> (9 * (level_of_paging - 1 - i))) & 0x1FF;
            if (pageTableEntries > 0)
            {
                paging_levels[i]++;
                num_page_tables += pageTableEntries;
            }
            
        }
        
        if(!level_2_prev_checking[level_indexes[1]])
        {
            paging_levels[1]++;
            //paging_levels[2]++;
            paging_levels[3]++;
            num_page_tables += 3;
        }
        level_2_prev_checking[level_indexes[1]] = 1;
        if(!level_3_prev_checking[level_indexes[1]][level_indexes[2]])
        {
            paging_levels[2]++;
            paging_levels[3]++;
            num_page_tables += 2;
        }
        level_3_prev_checking[level_indexes[1]][level_indexes[2]] = 1;
        if(!level_4_prev_checking[level_indexes[1]][level_indexes[2]][level_indexes[3]])
        {
            paging_levels[3]++;
            num_page_tables++;
        }
        level_4_prev_checking[level_indexes[1]][level_indexes[2]][level_indexes[3]] = 1;
    }

    fclose(pagemap);

    for(int i = 0; i < ENTRY_PER_PAGE; ++i)
    {
        for(int j = 0; j < ENTRY_PER_PAGE; ++j)
        {
            free(level_4_prev_checking[i][j]);
        }
        free(level_3_prev_checking[i]);
        free(level_4_prev_checking[i]);
    }
    free(level_2_prev_checking);
    free(level_3_prev_checking);
    free(level_4_prev_checking);

    // Calculate the size in kilobytes
    uint64_t pageTableSizeKB = num_page_tables * PAGESIZE / 1024;

    // Print the total page table size
    printf("(pid=%d) total memory occupied by 4-level page table: %lu KB (%d frames)\n",
           pid, pageTableSizeKB, num_page_tables);

    // Print the number of page tables at each level
    printf("(pid=%d) number of page tables used: level1=%d, level2=%d, level3=%d, level4=%d\n",
           pid, paging_levels[0], paging_levels[1], paging_levels[2], paging_levels[3]);
}*/

// 20 KB Hesapliyor 100 kb hesaplmasi lazim. pagetableentries yanlis hesapliyo
/*void alltablesize(int pid)
{
    int level_of_paging = 4;
    uint64_t paging_levels[4] = {0};
    char pagemap_file[64];
    FILE* pagemap;
    uint64_t start;
    uint64_t end;
    char dummy_permissions[5];

    sprintf(pagemap_file, "/proc/%d/maps", pid);

    pagemap = fopen(pagemap_file, "r");
    if (pagemap == NULL)
    {
        printf("Failed to open pagemap file\n");
        return;
    }

    while (fscanf(pagemap, "%lx-%lx %s %*x %*x:%*x %*d\n", &start, &end, dummy_permissions) == 3)
    {
        	
        	uint64_t mappingSize = end - start;
        	uint64_t numPageTableEntries = mappingSize / PAGESIZE;

        	// Level 1 (PML4)
        	paging_levels[0] += (numPageTableEntries + ENTRY_PER_PAGE * ENTRY_PER_PAGE * ENTRY_PER_PAGE - 1) / (ENTRY_PER_PAGE * ENTRY_PER_PAGE * ENTRY_PER_PAGE);

        	// Level 2 (PDP)
        	paging_levels[1] += (numPageTableEntries +( ENTRY_PER_PAGE * ENTRY_PER_PAGE) - 1) / (ENTRY_PER_PAGE * ENTRY_PER_PAGE) ;

        	// Level 3 (PD)
        	paging_levels[2] += (numPageTableEntries + (ENTRY_PER_PAGE) - 1) / ENTRY_PER_PAGE;


        	// Level 4 (PT)
        	paging_levels[3] += numPageTableEntries-1;
    }

    fclose(pagemap);

    uint64_t pageTableSizeKB = (paging_levels[0] + paging_levels[1] + paging_levels[2] + paging_levels[3]) * PAGESIZE / 1024;

    printf("(pid=%d) total memory occupied by 4-level page table: %lu KB (%lu frames)\n",
           pid, pageTableSizeKB, paging_levels[0] + paging_levels[1] + paging_levels[2] + paging_levels[3]);

    printf("(pid=%d) number of page tables used: level1=%lu, level2=%lu, level3=%lu, level4=%lu\n",
           pid, paging_levels[0], paging_levels[1], paging_levels[2], paging_levels[3]);
}
*/


uint64_t pfn_va_formatter(char* arg)
{
    uint64_t value;

    // Check if the input argument starts with "0x"
    if (arg[0] == '0' && arg[1] == 'x') {
        // Input is already in hexadecimal format
        value = strtoull(arg, NULL, 16);
    } else {
        // Convert decimal input to hexadecimal
        unsigned long decimal = strtoul(arg, NULL, 10);
        value = (uint64_t)decimal;
    }
    return value;
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
        frameinfo(pfn_va_formatter(argv[2]));
    } 
    else if (!strcmp(command, "-memused")) 
    {
        memused(atoi(argv[2]));
    } 
    else if (!strcmp(command, "-mapva")) 
    {
        mapva(atoi(argv[2]), pfn_va_formatter(argv[3]));
    } 
    else if (!strcmp(command, "-pte")) 
    {
        pte(atoi(argv[2]), pfn_va_formatter(argv[3]));
    } 
    else if (!strcmp(command, "-maprange")) 
    {
        maprange(atoi(argv[2]), pfn_va_formatter(argv[3]), pfn_va_formatter(argv[4]));
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
