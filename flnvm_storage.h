#ifndef _FLNVM_NAND_H
#define _FLNVM_NAND_H

#include "flnvm_block.h"
#include "flnvm_hil.h"

struct flnvm_storage {

        struct flnvm *flnvm;
        struct flnvm_hil *hil;

};



int flnvm_storage_setup_storage(struct flnvm_hil *hil);
int flnvm_storage_cleanup_storage(struct flnvm_hil *hil);

#endif
