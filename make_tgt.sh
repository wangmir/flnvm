sudo insmod flnvm.ko num_lun=4
sudo lnvm init -d flnvm
sudo lnvm create -d flnvm -n flnvm0 -t pblk -o 0:3
