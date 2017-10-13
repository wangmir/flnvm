
#ifndef _FLNVM_BLOCK_H
#define _FLNVM_BLOCK_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/lightnvm.h>


#include "flnvm_hil.h"
#include "flnvm_storage.h"

#define FLNVM_DEBUG
#ifdef FLNVM_DEBUG
#define flnvm_debug(fmt, a...) pr_info(fmt, ##a)
#else
#define flnvm_debug(fmt, a...) do{}while(0)
#endif

#define rqd_is_write(rqd) (rqd->opcode & 1)
#define rq_to_cmd(rq) (blk_mq_rq_to_pdu(rq))

struct flnvm {

        struct request_queue *q;
        struct blk_mq_tag_set tag_set;
        struct nvm_dev *ndev; // flnvm hil structure will be in ndev->private

        int flnvm_major;
        struct flnvm_hil *hil;
        struct kmem_cache *ppa_cache; // used for ppa_list cache on lnvm

        spinlock_t lock;

// param
        u32 hw_queue_depth;
        u32 num_sqs;
        u8 storage_gb;
        u8 num_channel;
        u8 num_lun;
        u8 num_pln;
        u16 num_blk;
        u16 num_pg;
        u16 fpg_sz;
        u8 is_nullblk;
//
        char disk_name[DISK_NAME_LEN];
};

#endif
