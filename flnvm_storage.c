#include "flnvm_storage.h"


static int flnvm_storage_data_free(struct flnvm_storage *storage){

}

static int flnvm_storage_data_alloc(struct flnvm_storage *storage){

}

static int flnvm_storage_struct_free(struct flnvm_storage *storage){

}

static int flnvm_storage_struct_alloc(struct flnvm_storage *storage){

        struct flnvm *flnvm = storage->hil->flnvm;

        storage->channel = (struct flnvm_channel *)kzalloc(sizeof(struct flnvm_channel) * flnvm->num_channel);
        if(!stoage->channel){
                pr_err("flnvm: storage channel struct allocation failed\n");
                return 1;
        }

        for(int i = 0; i < storage->channel; i++){
                struct flnvm_channel *channel = storage->channel;
                channel->lun = (struct flnvm_lun *)kzalloc(sizeof(struct flnvm_lun) * flnvm->num_lun);
                if(channel->lun){
                        pr_err("flnvm: storage lun struct allocation failed\n");
                        return 1;
                }
        }

        return 0;
}

int flnvm_storage_setup_storage(struct flnvm_hil *hil)
{

        struct flnvm_strage *storage;
        int ret = 0;

        pr_info("flnvm: flnvm storage setup\n");

        hil->storage = (struct flnvm_storage *)kzalloc(sizeof(struct flnvm_storage));
        if(!hil->storage){
                pr_err("flnvm: flnvm storage allocation failed\n");
                goto failed;
        }

        storage = hil->storage;
        storage->hil = hil;

        ret = flnvm_storage_struct_alloc(storage);
        if(ret)
                goto structure_failed;

        ret = flnvm_storage_data_alloc(data);
        if(ret)
                goto data_failed;

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
