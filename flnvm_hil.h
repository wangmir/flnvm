#ifndef _FLNVM_HIL_H
#define _FLNVM_HIL_H

#include "flnvm_block.h"
#include "flnvm_storage.h"

struct flnvm_cmd {
        struct list_head list;
        struct flnvm_queue *hq;
        u64 ppa_status;
        int error;
}

struct flnvm_work {
        struct work_struct work;
        void *cmd;
};

/* Submission queue structure for flnvm */
struct flnvm_queue {
        spin_lock_t lock;
        u32 queue_number;         // submission queue number
        struct workqueue_struct *workqueue; // implement hardware queue as idependent workqueue
        struct flnvm_work works;
        struct list_head cmd_list;
};

struct flnvm_hil {
        u32 queue_depth;
        u32 nr_queues;

        struct flnvm *flnvm;
        struct flnvm_queue *hqs;
        struct flnvm_storage *storage;

        spin_lock_t lock;
};


void flnvm_hil_end_cmd(struct flnvm_cmd *cmd);
int flnvm_hil_handle_cmd(struct flnvm_cmd *cmd);

void flnvm_hil_identify(struct flnvm *flnvm, struct nvm_id *id);
int flnvm_hil_get_l2p_tbl(struct flnvm *flnvm, u64 slba, u32 nlb,
        nvm_l2p_update_fn *update_l2p, void *priv);
int flnvm_hil_get_bb_tbl(struct flnvm *flnvm, struct ppa_addr ppa, u8 *blks);
int flnvm_hil_set_bb_tbl(struct flnvm *flnvm, struct ppa_addr *ppas,
        int nr_ppas, int type);

void flnvm_hil_init_queue(struct flnvm *flnvm, struct flnvm_queue *hq);
int flnvm_hil_setup_nvm(struct flnvm *flnvm);
void flnvm_hil_cleanup_nvm(struct flnvm *flnvm);

#endif
