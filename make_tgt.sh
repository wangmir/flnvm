sudo insmod flnvm.ko num_lun=4 nr_rwb=8 wt_nice=1 wt_pin=1 user_rb_option=1 gc_rb_option=1
#sudo insmod flnvm.ko num_lun=4
sudo nvme lnvm list
sudo modprobe pblk
sudo nvme lnvm create -d flnvm -n flnvm0 -t pblk --lun-begin=0 --lun-end=3 -f
