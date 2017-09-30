

#include <linux/lightnvm.h>

#include "flnvm_hil.h"

blk_status_t flnvm_hil_insert_cmd_to_hq(struct flnvm_cmd *cmd, struct flnvm_queue *hq){

        // pr_info("flnvm: hil_insert_cmd_to_hq\n");

        if(hq->num_cmd == hq->queue_depth)
                return BLK_STS_RESOURCE;

        list_add_tail(&cmd->list, &hq->cmd_list);
        hq->num_cmd++;
        queue_work(hq->hil->workqueue, &hq->work);

        return BLK_STS_OK;
}

void flnvm_hil_identify(struct flnvm *flnvm, struct nvm_id *id)
{
        struct nvm_id_group *grp;

        // pr_info("flnvm: flnvm_hil_identify\n");

        id->ver_id = 0x1;
        id->vmnt = 0;
        id->cap = cpu_to_le32(0x3);
        id->dom = 0;

        id->ppaf.sect_offset = 0;
        id->ppaf.sect_len = fls(cpu_to_le16(1) - 1); // sector per page = 1
        id->ppaf.pln_offset = id->ppaf.sect_offset + id->ppaf.sect_len;
        id->ppaf.pln_len = fls(cpu_to_le16(flnvm->num_pln) - 1);
        id->ppaf.pg_offset = id->ppaf.pln_offset + id->ppaf.pln_len;
        id->ppaf.pg_len = fls(cpu_to_le16(flnvm->num_pg) - 1);
        id->ppaf.blk_offset = id->ppaf.pg_offset + id->ppaf.pg_len;
        id->ppaf.blk_len = fls(cpu_to_le16(flnvm->num_blk) - 1);
        id->ppaf.lun_offset = id->ppaf.blk_offset + id->ppaf.blk_len;
        id->ppaf.lun_len = fls(cpu_to_le16(flnvm->num_lun) - 1);
        id->ppaf.ch_offset = id->ppaf.lun_offset + id->ppaf.lun_len;
        id->ppaf.ch_len = fls(cpu_to_le16(flnvm->num_channel) - 1);

	grp = &id->grp;
	grp->mtype = 0;
	grp->fmtype = 0;
	grp->num_ch = flnvm->num_channel;
	grp->num_pg = flnvm->num_pg;
	grp->num_lun = flnvm->num_lun;
	grp->num_blk = flnvm->num_blk;
	grp->num_pln = flnvm->num_pln;

	grp->fpg_sz = flnvm->fpg_sz;
	grp->csecs = flnvm->fpg_sz;

	grp->trdt = 25000;
	grp->trdm = 25000;
	grp->tprt = 500000;
	grp->tprm = 500000;
	grp->tbet = 1500000;
	grp->tbem = 1500000;

        if (grp->num_pln == 1)
                grp->mpos = 0x010101; /* single plane rwe */
        else if (grp->num_pln == 2)
                grp->mpos = 0x020202;
        else if (grp->num_pln == 4)
                grp->mpos = 0x040404;
        else{
                pr_err("flnvm: unsupported mpos\n");
                BUG();
        }

	grp->cpar = flnvm->hw_queue_depth;

	return;
}

int flnvm_hil_get_l2p_tbl(struct flnvm *flnvm, u64 slba, u32 nlb,
        nvm_l2p_update_fn *update_l2p, void *priv)
{
        // not used in pblk
        return 0;
}

int flnvm_hil_get_bb_tbl(struct flnvm *flnvm, struct ppa_addr ppa, u8 *blks)
{
        /*
                NVM_BLK_T_FREE		= 0x0, : free block
                NVM_BLK_T_BAD		= 0x1, : bad
                NVM_BLK_T_GRWN_BAD	= 0x2, : bad
                NVM_BLK_T_DEV		= 0x4, : not used in code
                NVM_BLK_T_HOST		= 0x8, : not used in code
        */

        // set 0 to all of bb tbl
        memset(blks, 0x00, flnvm->num_blk * flnvm->num_pln);
        return 0;
}

int flnvm_hil_set_bb_tbl(struct flnvm *flnvm, struct ppa_addr *ppas, int nr_ppas, int type)
{
        // it must not be called at emulation
        BUG();
        return 0;
}

static void flnvm_hil_end_cmd(struct flnvm_cmd *cmd)
{
        struct request *rq = cmd->rq;
        BUG_ON(!rq);

        cmd->end_rq(rq);
}

static void flnvm_hil_handle_io(struct flnvm_queue *hq, struct ppa_addr ppa, u8 opcode, struct page *page){

        if(hq->hil->flnvm->is_nullblk)
                return;

        switch(opcode){
                case NVM_OP_PWRITE:
                flnvm_storage_program(hq->hil, ppa, page_address(page));
                break;

                case NVM_OP_PREAD:
                flnvm_storage_read(hq->hil, ppa, page_address(page));
                break;

                case NVM_OP_ERASE:
                flnvm_storage_erase(hq->hil, ppa);
                break;

                default:
                pr_err("flnvm: flnvm_hil_handle_io error, invalid opcode\n");
                break;
        }
}

static void flnvm_hil_handle_cmd(struct flnvm_cmd *cmd)
{
        struct nvm_rq *rqd = cmd->rqd;
        struct bio *bio = rqd->bio;
        struct bio *pbio;
        struct bio_vec bvec;
        struct bvec_iter iter;
        int i = 0;

        // pr_info("flnvm: hil_handle_cmd\n");

        switch(rqd->opcode){
                case NVM_OP_PWRITE:
                case NVM_OP_PREAD:

                pbio = rqd->bio;
                bio_for_each_segment(bvec, pbio, iter){
                        if(rqd->nr_ppas == 1)
                                flnvm_hil_handle_io(cmd->hq, rqd->ppa_addr, rqd->opcode, bvec.bv_page);
                        else
                                flnvm_hil_handle_io(cmd->hq, rqd->ppa_list[i++], rqd->opcode, bvec.bv_page);
                }
                break;

                case NVM_OP_ERASE:
                flnvm_hil_handle_io(cmd->hq, rqd->ppa_addr, rqd->opcode, NULL);
                break;

                default:
                pr_err("flnvm: handle_cmd error, there are no available opcode\n");
                break;
        }

        flnvm_hil_end_cmd(cmd);
}

static void flnvm_hil_queue_work(struct work_struct *work){

        struct flnvm_queue *hq =
                container_of(work, struct flnvm_queue, work);

        // pr_info("flnvm: hil_queue_work\n");

        struct flnvm_cmd *cmd, *tcmd;

        list_for_each_entry_safe(cmd, tcmd, &hq->cmd_list, list){
                list_del(&cmd->list);

                // pr_info("flnvm: hil_queue_work handle cmd, num_cmd: %d\n", hq->num_cmd);
                hq->num_cmd--;
                flnvm_hil_handle_cmd(cmd);
        }
}

void flnvm_hil_init_queue(struct flnvm_hil *hil, struct flnvm_queue *hq)
{
        BUG_ON(!hil);
        BUG_ON(!hq);

        INIT_LIST_HEAD(&hq->cmd_list);
        hq->queue_depth = hil->queue_depth;
        hq->hil = hil;

        INIT_WORK(&hq->work, flnvm_hil_queue_work);
}

int flnvm_hil_setup_nvm(struct flnvm *flnvm)
{
        struct flnvm_hil *hil;
        int i, ret;

        pr_info("flnvm: hil_setup_nvm start\n");

        flnvm->hil = kzalloc(sizeof(struct flnvm_hil), GFP_KERNEL);
        if(!flnvm->hil){
                ret = -ENOMEM;
                pr_err("flnvm_hil alloc failed\n");
                goto out;
        }

        hil = flnvm->hil;

        // alloc workqueue: it will mimic hardware controller for SSD
        hil->workqueue = create_workqueue("flnvm_hq");
        if(!hil->workqueue){
                ret = -ENOMEM;
                pr_err("hil->workqueue alloc failed\n");
                goto hil_free;
        }

        hil->queue_depth = flnvm->hw_queue_depth;
        hil->nr_queues = flnvm->num_sqs;

        hil->hqs = kzalloc(hil->nr_queues * sizeof(struct flnvm_queue),
                GFP_KERNEL);
        if(!hil->hqs){
                ret = -ENOMEM;
                pr_err("hil->hqs alloc failed\n");
                goto workq_free;
        }

        for(i = 0; i < hil->nr_queues; i++)
                hil->hqs->queue_number = i;

        if(!flnvm->is_nullblk){
                ret = flnvm_storage_setup_storage(hil);
                if(ret){
                        goto queue_free;
                }
        }

        return 0;

queue_free:
        kfree(hil->hqs);
workq_free:
        destroy_workqueue(hil->workqueue);
hil_free:
        kfree(hil);
out:
        return ret;
}

static void flnvm_hil_cleanup_queue(struct flnvm_queue *q)
{
        return;
}

void flnvm_hil_cleanup_nvm(struct flnvm *flnvm)
{
        int i;

        flush_workqueue(hil->workqueue);

        for(i = 0; i < flnvm->hil->nr_queues; i++){
                flnvm_hil_cleanup_queue(&flnvm->hil->hqs[i]);
        }

        if(!flnvm->is_nullblk)
                flnvm_storage_cleanup_storage(flnvm->hil);

        kfree(flnvm->hil->hqs);
        destroy_workqueue(hil->workqueue);
        kfree(flnvm->hil);
}
