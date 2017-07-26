#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/types.h>

#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/lightnvm.h>
#include <linux/blk-mq.h>
#include <linux/hrtimer.h>
#include <linux/vmalloc.h>

#include "flnvm_block.h"

// Module parameters
static int hw_queue_depth = 64;
module_param(hw_queue_depth, int, S_IRUGO);
MODULE_PARM_DESC(hw_queue_depth, "Number of hardware queue");

static int num_sqs = 64;
module_param(num_sqs, int, S_IRUGO);
MODULE_PARM_DESC(num_sqs, "Number of submission queue");

static int storage_gb = 8;
module_param(storage_gb, int, S_IRUGO);
MODULE_PARM_DESC(storage_gb, "Size of storage in GB, if this value is non-zero, then num_chks will be ignored");

static int num_channel = 4;
module_param(num_channel, int, S_IRUGO);
MODULE_PARM_DESC(num_channel, "Number of channels per device");

static int num_lun = 4;
module_param(num_lun, int, S_IRUGO);
MODULE_PARM_DESC(num_lun, "Number of LUNs per channel");

static int num_pln = 4;
module_param(num_pln, int, S_IRUGO);
MODULE_PARM_DESC(num_pln, "Number of planes per LUN");

static int num_block = 0;
module_param(num_block, int, S_IRUGO);
MODULE_PARM_DESC(num_block, "Number of blocks per plane, if you wanna give the meaning on this value, need to set the value as zero on storage_gb");

static int num_pg = 128;
module_param(num_pg, int, S_IRUGO);
MODULE_PARM_DESC(num_pg, "Number of pages per block");

static int fpg_sz = 4096;
module_param(fpg_sz, int, S_IRUGO);
MODULE_PARM_DESC(fpg_sz, "Size of flash page in byte");
//

static int __init flnvm_init(void)
{


}

static void __exit flnvm_exit(void)
{


}

module_init(flnvm_init);
module_exit(flnvm_exit);

MODULE_AUTHOR("Hyukjoong Kim <wangmir@gmail.com>");
MODULE_LICENSE("GPL v2");
