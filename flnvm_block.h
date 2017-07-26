
#ifndef _FLNVM_BLOCK_H
#define _FLNVM_BLOCK_H

#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/lightnvm.h>

#include "flnvm_hil.h"
#include "flnvm_nand.h"


struct

struct flnvm {

  struct request_queue *q;
  struct gendisk *disk;
  struct nvm_dev *ndev;

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
//
  char disk_name[DISK_NAME_LEN];
};

#endif
