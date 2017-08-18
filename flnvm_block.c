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

static int num_channel = 1;
module_param(num_channel, int, S_IRUGO);
MODULE_PARM_DESC(num_channel, "Number of channels per device");

static int num_lun = 1;
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

static int is_nullblk = 1;
module_param(is_nullblk, int, S_IRUGO);
MODULE_PARM_DESC(is_nullblk, "if this value is set to 1, the driver will be init with nullblk");
//

// temporal pointer for exit
static struct flnvm *t_flnvm;

static void flnvm_end_request(struct request *rq){
        blk_mq_complete_request(rq);
}

static blk_status_t flnvm_queue_rq(struct blk_mq_hw_ctx *hctx,
        const struct blk_mq_queue_data *bd)
{
        blk_status_t ret;
        struct flnvm_cmd *cmd = rq_to_cmd(bd->rq);

        pr_info("flnvm: flnvm_queue_rq\n");

        cmd->rq = bd->rq;
        cmd->hq = hctx->driver_data;
        cmd->end_rq = flnvm_end_request;

        blk_mq_start_request(bd->rq);

        cmd->rqd = bd->rq->end_io_data;

        ret = flnvm_hil_insert_cmd_to_hq(cmd, cmd->hq);
        return ret;     // such as BLK_MQ_RQ_QUEUE_OK or BLK_MQ_RQ_QUEUE_BUSY, etc
}

static int flnvm_init_hctx(struct blk_mq_hw_ctx *hctx, void *data,
        unsigned int index)
{
        struct flnvm *flnvm = data;
        struct flnvm_queue *hq;

        if(!flnvm)
                BUG();
        if(!flnvm->hil)
                BUG();
        if(!flnvm->hil->hqs)
                BUG();

        hq = &flnvm->hil->hqs[index];
        hctx->driver_data = hq;

        flnvm_hil_init_queue(flnvm->hil, hq);
        flnvm->hil->nr_queues++;

        return 0;
}

static void flnvm_softirq_done_fn(struct request *rq)
{
        blk_mq_end_request(rq, 0);
}

static const struct blk_mq_ops flnvm_mq_ops = {
        .queue_rq       = flnvm_queue_rq,
        .init_hctx      = flnvm_init_hctx,
        .complete       = flnvm_softirq_done_fn,
};

static void flnvm_set_param(struct flnvm *flnvm){

        flnvm->hw_queue_depth = (u32)hw_queue_depth;
        flnvm->num_sqs = (u32)num_sqs;

        flnvm->num_channel = (u8)num_channel;
        flnvm->num_lun = (u8)num_lun;
        flnvm->num_pln = (u8)num_pln;

        flnvm->num_pg = (u16)num_pg;
        flnvm->fpg_sz = (u16)fpg_sz;

        if(storage_gb){
                sector_t storage_size;

                flnvm->storage_gb = (u8)storage_gb;
                storage_size = storage_gb * 1024 * 1024 * 1024ULL;
                sector_div(storage_size, fpg_sz);        // num total page
                sector_div(storage_size, num_channel);
                sector_div(storage_size, num_lun);
                sector_div(storage_size, num_pln);
                sector_div(storage_size, num_pg);       // num blocks per plane

                flnvm->num_blk = (u16)storage_size;
        }
        else {
                flnvm->num_blk = (u16)num_block;
        }

        flnvm->is_nullblk = (u8)is_nullblk;

        pr_info("flnvm: set_param, channel: %u, lun: %u, pln: %u\n", flnvm->num_channel, flnvm->num_lun, flnvm->num_pln);

        sprintf(flnvm->disk_name, "flnvm");
}

static int flnvm_nvm_identify(struct nvm_dev *ndev , struct nvm_id *id)
{
        flnvm_hil_identify(ndev->private_data, id);
        return 0;
}

static int flnvm_nvm_get_l2p_tbl(struct nvm_dev *ndev, u64 slba, u32 nlb,
        nvm_l2p_update_fn *update_l2p, void *priv)
{
        return flnvm_hil_get_l2p_tbl(ndev->private_data, slba, nlb, update_l2p, priv);
}

static int flnvm_nvm_get_bb_tbl(struct nvm_dev *ndev, struct ppa_addr ppa, u8 *blks)
{
        return flnvm_hil_get_bb_tbl(ndev->private_data, ppa, blks);
}

static int flnvm_nvm_set_bb_tbl(struct nvm_dev *ndev, struct ppa_addr *ppas,
        int nr_ppas, int type)
{
        return flnvm_hil_set_bb_tbl(ndev->private_data, ppas, nr_ppas, type);
}

static void flnvm_lnvm_end_io(struct request *rq, blk_status_t status)
{
        struct nvm_rq *rqd = rq->end_io_data;
        struct flnvm_cmd *cmd = rq_to_cmd(rq);

        rqd->ppa_status = cmd->ppa_status;
        rqd->error = cmd->error;
        nvm_end_io(rqd);

        blk_mq_free_request(rq);
}

static int flnvm_nvm_submit_io(struct nvm_dev *ndev, struct nvm_rq *rqd)
{
        struct request_queue *q = ndev->q;
        struct request *rq;
        struct bio *bio = rqd->bio;

        rq = blk_mq_alloc_request(q, rqd_is_write(rqd) ? REQ_OP_DRV_OUT : REQ_OP_DRV_IN, 0);
        if (IS_ERR(rq))
                return -ENOMEM;

        if(bio)
                blk_init_request_from_bio(rq, bio);
        else {
                rq->ioprio = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, IOPRIO_NORM);
                rq->__data_len = 0;
        }

        rq->end_io_data = rqd;

        blk_execute_rq_nowait(q, NULL, rq, 0, flnvm_lnvm_end_io);

        return 0;
}

static void *flnvm_nvm_create_dma_pool(struct nvm_dev *ndev, char *name)
{
        struct flnvm *flnvm = ndev->private_data;
        mempool_t *virtmem_pool;

	virtmem_pool = mempool_create_slab_pool(64, flnvm->ppa_cache);
	if (!virtmem_pool) {
		pr_err("flnvm: Unable to create virtual memory pool\n");
		return NULL;
	}

	return virtmem_pool;
}

static void flnvm_nvm_destroy_dma_pool(void *pool)
{
        mempool_destroy(pool);
}

static void *flnvm_nvm_dev_dma_alloc(struct nvm_dev *ndev, void *pool,
        gfp_t mem_flags, dma_addr_t *dma_handler)
{
        return mempool_alloc(pool, mem_flags);
}

static void flnvm_nvm_dev_dma_free(void *pool, void *entry, dma_addr_t dma_handler)
{
        mempool_free(entry, pool);
}

static struct nvm_dev_ops flnvm_nvm_dev_ops = {
        .identity       = flnvm_nvm_identify,

        .get_l2p_tbl    = flnvm_nvm_get_l2p_tbl,

        .get_bb_tbl     = flnvm_nvm_get_bb_tbl,
        .set_bb_tbl     = flnvm_nvm_set_bb_tbl,

        .submit_io      = flnvm_nvm_submit_io,

        .create_dma_pool        = flnvm_nvm_create_dma_pool,
        .destroy_dma_pool       = flnvm_nvm_destroy_dma_pool,
        .dev_dma_alloc          = flnvm_nvm_dev_dma_alloc,
        .dev_dma_free           = flnvm_nvm_dev_dma_free,

        .max_phys_sect          = 64,           // not 512 byte sector, but ppa pages
};

static int flnvm_nvm_register(struct flnvm *flnvm)
{
        struct nvm_dev *dev;
        int ret;

        dev = nvm_alloc_dev(0);
        if(!dev)
                return -ENOMEM;

        dev->q = flnvm->q;
        memcpy(dev->name, flnvm->disk_name, DISK_NAME_LEN);
        dev->private_data = flnvm;
        dev->ops = &flnvm_nvm_dev_ops;

        ret = nvm_register(dev);
        if(ret){
                kfree(dev);
                return ret;
        }
        flnvm->ndev = dev;
        return 0;
}

static void flnvm_nvm_unregister(struct flnvm *flnvm)
{
        nvm_unregister(flnvm->ndev);
}

static void flnvm_dev_destroy(struct flnvm *flnvm)
{
        flnvm_nvm_unregister(flnvm);
        blk_cleanup_queue(flnvm->q);
        blk_mq_free_tag_set(&flnvm->tag_set);
        flnvm_hil_cleanup_nvm(flnvm);
        kfree(flnvm);
}

static int flnvm_dev_init(struct flnvm *flnvm, int major)
{
        int ret;

        pr_info("flnvm: dev init start\n");

        flnvm->flnvm_major = major;
        flnvm_set_param(flnvm);

        // ppa_list cache. maximum number (64) of vectored I/O
        flnvm->ppa_cache = kmem_cache_create("ppa_cache", 64 * sizeof(u64), 0, 0, NULL);
        if(!flnvm->ppa_cache){
                pr_err("flnvm: failed to create ppa_cache\n");
                ret = -ENOMEM;
                goto out;
        }

        spin_lock_init(&flnvm->lock);

        ret = flnvm_hil_setup_nvm(flnvm);
        if(ret)
                goto setup_nvm_err;

        // set tag_set
        flnvm->tag_set.ops = &flnvm_mq_ops;
        flnvm->tag_set.nr_hw_queues = flnvm->num_sqs;
        flnvm->tag_set.queue_depth = hw_queue_depth;
        flnvm->tag_set.numa_node = NUMA_NO_NODE;        // do we need to fix it?
        flnvm->tag_set.cmd_size = sizeof(struct flnvm_cmd);
        flnvm->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;   // do we need to add BLK_MQ_F_BLOCKING?
        flnvm->tag_set.driver_data = flnvm;

        ret = blk_mq_alloc_tag_set(&flnvm->tag_set);
        if(ret)
                goto cleanup_nvm;

        flnvm->q = blk_mq_init_queue(&flnvm->tag_set);
        if(IS_ERR(flnvm->q)) {
                ret = -ENOMEM;
                goto cleanup_tags;
        }

        flnvm->q->queuedata = flnvm;
        queue_flag_set_unlocked(QUEUE_FLAG_NONROT, flnvm->q);
        queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, flnvm->q);

        blk_queue_logical_block_size(flnvm->q, 512);    // sector size
        blk_queue_physical_block_size(flnvm->q, flnvm->fpg_sz);

        ret = flnvm_nvm_register(flnvm);
        if(ret){
                pr_err("flnvm: flnvm_nvm_register failed\n");
                goto cleanup_blk_queue;
        }
        return 0;

cleanup_blk_queue:
        blk_cleanup_queue(flnvm->q);
cleanup_tags:
        blk_mq_free_tag_set(&flnvm->tag_set);
cleanup_nvm:
        flnvm_hil_cleanup_nvm(flnvm);
setup_nvm_err:
        kmem_cache_destroy(flnvm->ppa_cache);
out:
        pr_err("flnvm: dev_init out with error\n");
        return ret;
}

static int __init flnvm_init(void)
{
        int ret = 0;
        int dev_major;
        struct flnvm *flnvm = NULL;
        struct nvm_dev *ndev = NULL;

        dev_major = register_blkdev(0, "flnvm");
        if(dev_major < 0){
                pr_err("Blkdev registering failed\n");
                return dev_major;
        }

        flnvm = kzalloc(sizeof(struct flnvm), GFP_KERNEL);
        if(!flnvm){
                ret = -ENOMEM;
                goto out;
        }

        ret = flnvm_dev_init(flnvm, dev_major);
        if(ret)
                goto err_dev;

        t_flnvm = flnvm;
        ndev = flnvm->ndev;
        if(!t_flnvm)
                pr_err("flnvm: t_flnvm is NULL, %x\n", flnvm);

        // init complete
        pr_info("flnvm: fake lightnvm device driver loaded\n");
        pr_info("flnvm: num_channel: %d", ndev->geo.nr_chnls);
        pr_info("flnvm: num_lun: %d", ndev->geo.nr_luns);
        pr_info("flnvm: luns_per_chnl: %d", ndev->geo.luns_per_chnl);
        pr_info("flnvm: num_planes: %d", ndev->geo.nr_planes);
        pr_info("flnvm: sec_per_pg: %d", ndev->geo.sec_per_pg);
        pr_info("flnvm: pgs_per_blk: %d", ndev->geo.pgs_per_blk);
        pr_info("flnvm: blks_per_lun: %d", ndev->geo.blks_per_lun);
        pr_info("flnvm: fpg_size: %d", ndev->geo.fpg_size);
        pr_info("flnvm: pfpg_size: %d\n", ndev->geo.pfpg_size);
        pr_info("flnvm: sec_per_pl: %d\n", ndev->geo.sec_per_pl);
        pr_info("flnvm: sec_size: %d\n", ndev->geo.sec_size);

        pr_info ("flnvm: init fin\n");
        return 0;

err_dev:
        flnvm_dev_destroy(flnvm);
        unregister_blkdev(dev_major, "flnvm");
        kfree(flnvm);
out:
        return ret;
}

static void __exit flnvm_exit(void)
{
        pr_info("flnvm: fake lightnvm device driver exit start\n");

        if(!t_flnvm)
                pr_err("flnvm: t_flnvm is NULL\n");

        unregister_blkdev(t_flnvm->flnvm_major, "flnvm");
        flnvm_dev_destroy(t_flnvm);
}

module_init(flnvm_init);
module_exit(flnvm_exit);

MODULE_AUTHOR("Hyukjoong Kim <wangmir@gmail.com>");
MODULE_LICENSE("GPL v2");
