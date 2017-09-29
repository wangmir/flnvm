#ifndef _FLNVM_NAND_H
#define _FLNVM_NAND_H

#include "flnvm_block.h"
#include "flnvm_hil.h"

struct flnvm_page{
        unsigned int *data;
};

struct flnvm_block{
        struct list_head blk_list;
        struct flnvm_page *page;
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
};

int flnvm_storage_program(struct flnvm_hil *hil, struct ppa_addr ppa, struct page *page);
int flnvm_storage_read(struct filnvm_hil *hil, struct ppa_addr ppa, struct page *page);
int flnvm_storage_erase(struct flnvm_hil *hil, struct ppa_addr ppa);

int flnvm_storage_setup_storage(struct flnvm_hil *hil);
int flnvm_storage_cleanup_storage(struct flnvm_hil *hil);

#endif
