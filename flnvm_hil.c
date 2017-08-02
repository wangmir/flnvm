

#inlcude <linux/lightnvm.h>

#include "flnvm_hil.h"

void flnvm_hil_identify(struct *flnvm, struct nvm_id *id)
{

}

int flnvm_hil_get_l2p_tbl(struct flnvm *flnvm, u64 slba, u32 nlb,
        nvm_l2p_update_fn *update_l2p, void *priv)
{

}

int flnvm_hil_get_bb_tbl(struct flnvm *flnvm, struct ppa_addr ppa, u8 *blks)
{


}

int flnvm_hil_set_bb_tbl(struct *flnvm, struct ppa_addr *ppas, int nr_ppas, int type)
{


}

void flnvm_hil_init_queue(struct flnvm *flnvm, struct flnvm_queue *hq)
{
        // queue work에 대한 init 작업 진행
        INIT_LIST_HEAD(&flnvm->cmd_list);

        
}

int flnvm_hil_setup_nvm(struct flnvm *flnvm)
{
        struct flnvm_hil *hil;
        int ret;

        hil = kzalloc(sizeof(struct flnvm_hil), GFP_KERNEL);
        if(!hil){
                ret = -ENOMEM;
                pr_err("flnvm_hil alloc failed\n");
                goto out;
        }

        hil->queue_depth = flnvm->hw_queue_depth;
        hil->nr_queues = flnvm->num_sqs;

        hil->hqs = kzalloc(hil->nr_queues * sizeof(struct flnvm_queue),
                GFP_KERNEL);
        if(!hil->hqs){
                ret = -ENOMEM;
                pr_err("hil->hqs alloc failed\n");
                goto queue_free;
        }

        ret = flnvm_storage_setup_storage(flnvm_hil);
        if(ret){
                goto queue_free;
        }

        return 0;

queue_free:
        kfree(hil->hqs);
hil_free:
        kfree(hil);
out:
        return ret;
}

static void flnvm_hil_cleanup_queue(struct flnvm_queue *q)
{

}

void flnvm_hil_cleanup_nvm(struct flnvm *flnvm)
{
        int i;

        for(i = 0; i < flnvm->hil->nr_queues; i++){
                flnvm_hil_cleanup_queue(&flnvm->hil->hqs[i]);
        }

        kfree(flnvm->hil->hqs);
        kfree(flnvm->hil);
}
