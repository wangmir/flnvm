#ifndef _FLNVM_NAND_H
#define _FLNVM_NAND_H

#include "flnvm_block.h"
#include "flnvm_hil.h"

struct flnvm_page{
        char data[4096];
};

#define FLNVM_PAGE_ORDER 8 // 256

struct flnvm_block{
        struct flnvm_page *page; // 256 = num_pg
};

struct flnvm_pln {
        struct flnvm_block *block;
};

struct flnvm_lun{
        struct flnvm_pln *plane;
};

struct flnvm_channel {
        struct flnvm_lun *lun;
};

struct flnvm_storage {
        struct flnvm_hil *hil;
        struct flnvm_channel *channel;

        int total_blks;
        int blk_order; // page order of the size of physical blocks
        void *blk_ptr;

        struct list_head block_list;
        struct list_head allocated_block_list;
};

int flnvm_storage_program(struct flnvm_hil *hil, struct ppa_addr ppa, struct page *page);
int flnvm_storage_read(struct flnvm_hil *hil, struct ppa_addr ppa, struct page *page);
int flnvm_storage_erase(struct flnvm_hil *hil, struct ppa_addr ppa);

int flnvm_storage_setup_storage(struct flnvm_hil *hil);
int flnvm_storage_cleanup_storage(struct flnvm_hil *hil);

#endif
