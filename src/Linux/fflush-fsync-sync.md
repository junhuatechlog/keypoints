Reset Safe Ram Disk
----
rpram: reset safe ram disk
```c
    /src-rfs/src/rootfs/systemd/rpram.service   
    ExecStart=/etc/init.d/ramdisk
```
phram MTD devices
```c
dev_nvrd_name="/dev/nvrd"

fsck_logfile="/fsck.nvrd.log"

fsck_ret_value="/fsck.ret"
```
```shell
# do the check in a subshell and store the return value in a file
( fsck.ext4 -y -f $dev_nvrd_name &> $fsck_logfile; echo $? > ${fsck_ret_value}; ) &
fsck_pid=$!
```
```c
mount -o bind /nvrd/log /var/log
mount -o bind /nvrd/tmp /var/tmp
```

ext4 usese hash table tree(HTREE) for directory organization and the extents organization.
Check if hash tree enabled:
tune2fs -l /dev/sde3 | grep dir_index

    -bash-4.3# tune2fs -l /dev/nvrd 
    tune2fs 1.42.11 (09-Jul-2014)
    Filesystem volume name:   var
    Last mounted on:          /var/log
    Filesystem UUID:          ffc73ec9-b5fc-431c-8971-cb950f9e42f1
    Filesystem magic number:  0xEF53
    Filesystem revision #:    1 (dynamic)
    Filesystem features:      ext_attr resize_inode dir_index filetype extent flex_bg sparse_super huge_file uninit_bg dir_nlink extra_isize
    Filesystem flags:         signed_directory_hash 
    Default mount options:    user_xattr acl
    Filesystem state:         not clean
    Errors behavior:          Continue
    Filesystem OS type:       Linux
    Inode count:              16384
    Block count:              65536
    Reserved block count:     3276
    Free blocks:              41422
    Free inodes:              16129
    First block:              1
    Block size:               1024
    Fragment size:            1024
    Reserved GDT blocks:      255
    Blocks per group:         8192
    Fragments per group:      8192
    Inodes per group:         2048
    Inode blocks per group:   256
    Flex block group size:    16
    Filesystem created:       Tue Jan  1 00:00:02 2013
    Last mount time:          Tue Jan  1 00:00:01 2013
    Last write time:          Tue Jan  1 00:00:01 2013
    Mount count:              1
    Maximum mount count:      -1
    Last checked:             Tue Jan  1 00:00:00 2013
    Check interval:           0 (<none>)
    Lifetime writes:          13 MB
    Reserved blocks uid:      0 (user root)
    Reserved blocks gid:      0 (group root)
    First inode:              11
    Inode size:           128
    Default directory hash:   half_md4
    Directory Hash Seed:      c6f0671c-cd32-4c35-aa4f-48ec91fc877c
    Journal backup:           inode blocks



### Pronto phenomenon:

#### nvrd.log:

1. log:
Problem in HTREE directory inode 50 (/tmp/hwapi): bad block number 3.
Clear HTree index? yes

2. log:
Pass 2: Checking directory structure
Entry 'tuningHistory.log' in /tmp (2049) references inode 151 found in group 0's unused inodes area.
Fix? yes

Problem in HTREE directory inode 50 (/tmp/hwapi): bad block number 3.
Clear HTree index? yes

Restarting e2fsck from the beginning...
One or more block group descriptor checksums are invalid.  Fix? yes

#### hwapi log:
```c
Line 2486: 51000 002486 18.08 20:55:29.864502956  [192.168.129.2]  17 P-1111-0-HWRRese <2016-08-18T11:43:24.742445Z> DE0-RSTMain INF/HWA/RESET, ApiResetRpramSet: Writing 0x10110930 : to /var/tmp/hwapi/RP_RAM_hdbde_test_mode.txt
```

Pass 2: Checking directory structure
Entry `test1233.log` in /tmp/hwapi (66) has deleted/unused inode 52.  Clear? yes

http://man7.org/linux/man-pages/man5/ext4.5.html

#### has_journal
       Create a journal to ensure filesystem consistency even across
       unclean shutdowns.  Setting the filesystem feature is
       equivalent to using the -j option with mke2fs or tune2fs.
       This feature is supported by ext3 and ext4, and ignored by the
       ext2 file system driver.

#### Journal checksumming
 The journal is the most used part of the disk, making the blocks that form part of it more prone to hardware failure. And recovering from a corrupted journal can lead to massive corruption. Ext4 checksums the journal data to know if the journal blocks are failing or corrupted. But journal checksumming has a bonus: it allows one to convert the two-phase commit system of Ext3's journaling to a single phase, speeding the filesystem operation up to 20% in some cases - so reliability and performance are improved at the same time. (Note: the part of the feature that improves the performance, the asynchronous logging, is turned off by default for now, and will be enabled in future releases, when its reliability improves)
 "No Journaling" mode

 Journaling ensures the integrity of the filesystem by keeping a log of the ongoing disk changes. However, it is known to have a small overhead. Some people with special requirements and workloads can run without a journal and its integrity advantages. In Ext4 the journaling feature can be disabled, which provides a small performance improvement. 

In a journaling filesystem a special file called a journal is used to repair any inconsistencies caused due to improper shutdown of a system

https://en.wikipedia.org/wiki/Ext3#Journaling_levels


       data={journal|ordered|writeback}
              Specifies the journaling mode for file data.  Metadata is
              always journaled.  To use modes other than ordered on the root
              filesystem, pass the mode to the kernel as boot parameter,
              e.g. rootflags=data=journal.

              journal
                     All data is committed into the journal prior to being
                     written into the main filesystem.

              ordered
                     This is the default mode.  All data is forced directly
                     out to the main file system prior to its metadata being
                     committed to the journal.

              writeback
                     Data ordering is not preserved ????data may be written
                     into the main filesystem after its metadata has been
                     committed to the journal.  This is rumoured to be the
                     highest-throughput option.  It guarantees internal
                     filesystem integrity, however it can allow old data to
                     appear in files after a crash and journal recovery.

---------------------------------------
```c
mount -t ext4 -o sync,rw,noatime,exec,data=writeback $dev_nvrd $nvrd_mp
```
mount options:

    sync   All I/O to the filesystem should be done synchronously. In case of media with limited number of write cycles (e.g. some flash  drives)  "sync"  may  cause life-cycle shortening.

write-caching disabled, you can safely disable write barriers at mount time using the -o nobarrier option for mount

### Mount options for ext4
     barrier=none / barrier=flush
              This enables/disables the use of write barriers in the journaling code.  barrier=none disables it, barrier=flush enables it. Write barriers enforce proper
              on-disk ordering of journal commits, making volatile disk write caches safe to use, at some performance penalty. The reiserfs filesystem does  not  enable
              write barriers by default. Be sure to enable barriers unless your disks are battery-backed one way or another. Otherwise you risk filesystem corruption in
              case of power failure.

fsync()/sync maybe can solve the problem

fsync, fdatasync - synchronize a file's in-core state with storage device

 fsync()  transfers  ("flushes")  all  modified in-core data of (i.e., modified buffer cache pages for) the file referred to by the file descriptor fd to the disk
       device (or other permanent storage device) so that all changed information can be retrieved even after the system crashed or was rebooted.  This includes writing
       through  or  flushing  a  disk cache if present.  The call blocks until the device reports that the transfer has completed.  It also flushes metadata information
       associated with the file (see stat(2)).

### Difference between fflush(), fsync() and sync

***fflush()*** works on FILE* , it just flushes the internal buffers in the FILE* of your application out to the OS.

***fsync*** works on a lower level, it tells the OS to flush its buffers to the physical media.

OSs heavily caches data you write to a file. If the OS enforced every write to hit the drive, things would be very slow. fsync(among other things) allows you to control when the data should hit the drive.

Furthermore, fsync works on a file descriptor. It has no knowledge of a FILE* and can't flush its buffers. FILE* lives in your application, file descriptors lives in the OS kernel, typically.

### To force the commitment of recent changes to disk, use the sync() or fsync() functions. 

***fsync()*** will synchronize all of the given file's data and metadata with the permanent storage device. It should be called just before the corresponding file has been closed. 

***sync()*** will commit all modified files to disk.

fdatasync与fsync的区别在于fdatasync不会flush文件的metadata信息。这个是为了减少对磁盘的操作。
If the underlying hard disk has write caching enabled , then the data may not really be on permanent storage when fsync() / fdatasync() return(4.3BSD, POSIX.1-2001).
但是在我的linux系统man里，没有发现这句话。 

总的来说， fflush()只是将流刷出application到OS，不一定到disk, fsync()将特定文件(fd)刷到disk，sync()就是刷所有文件了。 
