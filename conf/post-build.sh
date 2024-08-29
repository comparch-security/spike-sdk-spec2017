TARGETDIR=$1
BR_ROOT=$PWD

# setup mount for 9p FS tag /dev/boot
echo '/dev/root /root/spec 9p rw,relatime,access=client,msize=8192,trans=virtio 0 0' >> $TARGETDIR/etc/fstab

