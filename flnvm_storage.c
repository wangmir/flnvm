#include "flnvm_storage.h"


int flnvm_storage_program(struct flnvm_hil *hil, struct ppa_addr ppa, struct page *page){

        void *data, *dst;
        unsigned long *test;
        struct flnvm_storage *storage = hil->storage;

        unsigned int ch = ppa.g.ch, lun = ppa.g.lun, pl = ppa.g.pl;
        unsigned int sec = ppa.g.sec, blk = ppa.g.blk, pg = ppa.g.pg;

        data = (void *)storage->channel[ch].lun[lun].plane[pl].block[blk].page[pg].data;

        dst = kmap_atomic(page);

        test = (unsigned long *)dst;

        flnvm_debug("flnvm: storage_program, ch: %u, lun: %u, pl: %u, blk: %u, pg: %u, data: %u\n"
                , ch, lun, pl, blk, pg, test[0]);

        copy_from_user((void *)data, dst, 4096);
        // memcpy(data, dst, 4096);

        kunmap_atomic(dst);
        return 0;
}

int flnvm_storage_read(struct flnvm_hil *hil, struct ppa_addr ppa, struct page *page){

        void *data, *dst;
        struct flnvm_storage *storage = hil->storage;
        unsigned long *test;

        unsigned int ch = ppa.g.ch, lun = ppa.g.lun, pl = ppa.g.pl;
        unsigned int sec = ppa.g.sec, blk = ppa.g.blk, pg = ppa.g.pg;

        // pr_info("flnvm: storage_read\n");

        data = (void *)storage->channel[ch].lun[lun].plane[pl].block[blk].page[pg].data;

        dst = kmap_atomic(page);

        test = (unsigned long *)data;

        flnvm_debug("flnvm: storage_read, ch: %u, lun: %u, pl: %u, blk: %u, pg: %u, data: %u\n"
                , ch, lun, pl, blk, pg, test[0]);

        copy_to_user(dst, (void *)data, 4096);
        //memcpy(dst, data, 4096);

        kunmap_atomic(dst);
        return 0;
}


int flnvm_storage_erase(struct flnvm_hil *hil, struct ppa_addr ppa){

        return 0;
}

static int flnvm_storage_data_free(struct flnvm_storage *storage){

        struct flnvm *flnvm = storage->hil->flnvm;
        int ret = 0, i, j, k, l;

        for(i = 0; i < flnvm->num_channel; i++){
                for(j = 0; j < flnvm->num_lun; j++){
                        for(k = 0; k < flnvm->num_pln; k++){
                                for(l = 0; l < flnvm->num_blk; l++){
                                        free_pages((unsigned long)storage->channel[i].lun[j].plane[k].block[l].page, FLNVM_PAGE_ORDER);
                                }
                        }
                }
        }

        return 0;
}

static int flnvm_storage_data_alloc(struct flnvm_storage *storage){

        struct flnvm *flnvm = storage->hil->flnvm;
        int ret = 0;
        int i, j, k, l;

        for(i = 0; i < flnvm->num_channel; i++){
                for(j = 0; j < flnvm->num_lun; j++){
                        for(k = 0; k < flnvm->num_pln; k++){
                                for(l = 0; l < flnvm->num_blk; l++){
                                        // what kind of malloc can be useful for this?
                                        storage->channel[i].lun[j].plane[k].block[l].page =
                                                (struct flnvm_page *)__get_free_pages(GFP_KERNEL, FLNVM_PAGE_ORDER);
                                        if(!storage->channel[i].lun[j].plane[k].block[l].page){
                                                pr_err("flnvm: flnvm storage data alloc failed\n");
                                                return 1;
                                        }
                                }
                        }
                }
        }
        return 0;
}

static int flnvm_storage_struct_free(struct flnvm_storage *storage){

        struct flnvm *flnvm = storage->hil->flnvm;
        int i, j, k;

        if(!storage->channel)
                return 0;

        for(i = 0; i < flnvm->num_channel; i++){

                if(!storage->channel[i].lun)
                        break;

                for(j = 0; j < flnvm->num_lun; j++){

                        if(!storage->channel[i].lun[j].plane)
                                break;
                        for(k = 0; k < flnvm->num_pln; k++){
                                kfree(storage->channel[i].lun[j].plane[k].block);
                        }
                        kfree(storage->channel[i].lun[j].plane);
                }
                kfree(storage->channel[i].lun);
        }
        kfree(storage->channel);

        return 0;
}

static int flnvm_storage_struct_alloc(struct flnvm_storage *storage){

        struct flnvm *flnvm = storage->hil->flnvm;
        int i, j, k;

        storage->channel = (struct flnvm_channel *)kzalloc(sizeof(struct flnvm_channel) * flnvm->num_channel, GFP_KERNEL);
        if(!storage->channel){
                pr_err("flnvm: storage channel struct allocation failed\n");
                return 1;
        }

        for(i = 0; i < flnvm->num_channel; i++){
                struct flnvm_channel *channel = &storage->channel[i];
                channel->lun = (struct flnvm_lun *)kzalloc(sizeof(struct flnvm_lun) * flnvm->num_lun, GFP_KERNEL);
                if(!channel->lun){
                        pr_err("flnvm: storage lun struct allocation failed\n");
                        return 1;
                }

                for(j = 0; j < flnvm->num_lun; j++){
                        struct flnvm_lun *lun = &channel->lun[j];
                        lun->plane = (struct flnvm_pln *)kzalloc(sizeof(struct flnvm_pln) * flnvm->num_pln, GFP_KERNEL);
                        if(!lun->plane){
                                pr_err("flnvm: storage pln struct allocation failed \n");
                                return 1;
                        }

                        for(k = 0; k < flnvm->num_pln; k++){
                                struct flnvm_pln *plane = &lun->plane[k];
                                plane->block = (struct flnvm_block *)kzalloc(sizeof(struct flnvm_pln) * flnvm->num_blk, GFP_KERNEL);
                                if(!plane->block){
                                        pr_err("flnvm: storage block struct allocation failed\n");
                                        return 1;
                                }
                        }
                }
        }
        return 0;
}

int flnvm_storage_cleanup_storage(struct flnvm_hil *hil){

        struct flnvm *flnvm = hil->flnvm;
        struct flnvm_storage *storage = hil->storage;

        pr_info("flnvm: flnvm storage cleanup\n");

        flnvm_storage_data_free(storage);
        flnvm_storage_struct_free(storage);
        kfree(storage);
        return 0;
}

int flnvm_storage_setup_storage(struct flnvm_hil *hil)
{
        struct flnvm *flnvm = hil->flnvm;
        struct flnvm_storage *storage;
        int ret = 0;

        pr_info("flnvm: flnvm storage setup\n");

        hil->storage = (struct flnvm_storage *)kzalloc(sizeof(struct flnvm_storage), GFP_KERNEL);
        if(!hil->storage){
                pr_err("flnvm: flnvm storage allocation failed\n");
                goto failed;
        }

        storage = hil->storage;
        storage->hil = hil;

        storage->total_blks = flnvm->num_blk * flnvm->num_pln * flnvm->num_lun * flnvm->num_channel;

        INIT_LIST_HEAD(&storage->block_list);
        INIT_LIST_HEAD(&storage->allocated_block_list);

        ret = flnvm_storage_struct_alloc(storage);
        if(ret)
                goto structure_failed;

        ret = flnvm_storage_data_alloc(storage);
        if(ret)
                goto data_failed;

        pr_info("flnvm: storage setup fin\n");

        return 0;

data_failed:
        pr_err("flnvm: flnvm storage data alloc failed\n");
        flnvm_storage_data_free(storage);
structure_failed:
        pr_err("flnvm: flnvm storage struct alloc failed\n");
        flnvm_storage_struct_free(storage);
        kfree(hil->storage);
failed:
        return 1;
}
