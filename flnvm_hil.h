#ifndef _FLNVM_HIL_H
#define _FLNVM_HIL_H

#include "flnvm_block.h"
#include "flnvm_storage.h"

struct flnvm_cmd {
        struct list_head list;
        struct flnvm_queue *hq;
        struct request *rq;
        struct nvm_rq *rqd;
        u64 ppa_status;
        int error;

        end_request *end_rq;
}


/* Submission queue structure for flnvm */
struct flnvm_queue {
        struct flnvm_hil *hil;

        spin_lock_t lock;
        u32 queue_number;         // submission queue number
        u32 queue_depth;

        struct work_struct work;
        struct list_head cmd_list;
        u32 num_cmd;
};

struct flnvm_hil {
        u32 queue_depth;
        u32 nr_queues;

        struct flnvm *flnvm;
        struct flnvm_queue *hqs;
        struct flnvm_storage *storage;

        /*
        there are many possible solutions to emulate storage work
        we can allocate individual single threaded workqueue for each flnvm_queue,
        or, we also can make order workqueue and share the per-core workqueue within flnvm_queues
        */
        struct workqueue_struct *workqueue;

        spin_lock_t lock;
};

int flnvm_hil_insert_cmd_to_hq(struct flnvm_cmd *cmd, struct flnvm_queue *hq);

void flnvm_hil_identify(struct flnvm *flnvm, struct nvm_id *id);
int flnvm_hil_get_l2p_tbl(struct flnvm *flnvm, u64 slba, u32 nlb,
        nvm_l2p_update_fn *update_l2p, void *priv);
int flnvm_hil_get_bb_tbl(struct flnvm *flnvm, struct ppa_addr ppa, u8 *blks);
int flnvm_hil_set_bb_tbl(struct flnvm *flnvm, struct ppa_addr *ppas,
        int nr_ppas, int type);

void flnvm_hil_init_queue(struct flnvm_hil *hil, struct flnvm_queue *hq);
int flnvm_hil_setup_nvm(struct flnvm *flnvm);
void flnvm_hil_cleanup_nvm(struct flnvm *flnvm);

#endif
