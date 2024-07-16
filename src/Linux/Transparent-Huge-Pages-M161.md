Transparent Huge Page support
-----
### Issue description:
Transport team have a Mansho that IPsec process(charon which have 520 threads) will be killed because of OOM(out of memory) after startup soon. 
Root cause is it accupy 1G bytes abnormally, while 100 Mbytes normally. 
From /proc/`pid of charon`/smaps show every thread occupy 2 Mbytes AnonHugePages. 
```c
    fee7a06000-fee8205000 rwxp 00000000 00:00 0                              [stack:13630]
    Size:               8188 kB
    Rss:                2068 kB
    Pss:                2068 kB
    Shared_Clean:          0 kB
    Shared_Dirty:          0 kB
    Private_Clean:         0 kB
    Private_Dirty:      2068 kB
    Referenced:         2068 kB
    Anonymous:          2068 kB
    AnonHugePages:      2048 kB
    Swap:                  0 kB
    KernelPageSize:        4 kB
    MMUPageSize:           4 kB
    Locked:                0 kB
    VmFlags: rd wr ex mr mw me ac
    fee8205000-fee8206000 ---p 00000000 00:00 0
    Size:                  4 kB
    Rss:                   0 kB
    Pss:                   0 kB
    Shared_Clean:          0 kB
    Shared_Dirty:          0 kB
    Private_Clean:         0 kB
    Private_Dirty:         0 kB
    Referenced:            0 kB
    Anonymous:             0 kB
    AnonHugePages:         0 kB
    Swap:                  0 kB
    KernelPageSize:        4 kB
    MMUPageSize:           4 kB
    Locked:                0 kB
    VmFlags: mr mw me ac
    ...
```
### Investigation:
Found **Transparent Huge Page** feature is enabled in system wide.  
It will cause the system to allocate 2 Mbytes first if possible, but according to the document in LWN, 
the extra memory will be splitted and reclaimed by the system. There should be no such case. 
Write a simple test program to simulate the 520 threads malloc 120 bytes, the issue can be easily reproduced. 
Both on the hzling40, LRC, and own PC.

### RootCause:
ZhouLB gives an explanation, When the THP feature enabled in system wide,  the new created thread malloc 120 bytes,
the glibc's malloc will map memory according to if the thread address is aligned with 2 Mbytes. 
If the thread address is aligned to 2Mbytes, it will map 2 Mbytes physical memory, if not aligned, it will map 4 Kbytes.
```c
    ffe3001000-ffe3800000 rwxp 00000000 00:00 0                              [stack:16400] //aligned to 2Mbytes
    Size:               8188 kB
    Rss:                2048 kB
    Pss:                2048 kB
    Shared_Clean:          0 kB
    Shared_Dirty:          0 kB
    Private_Clean:         0 kB
    Private_Dirty:      2048 kB
    Referenced:         2048 kB
    Anonymous:          2048 kB
    AnonHugePages:      2048 kB
    Swap:                  0 kB
    KernelPageSize:        4 kB
    MMUPageSize:           4 kB
    Locked:                0 kB
    VmFlags: rd wr ex mr mw me ac

    ffe8a55000-ffe9254000 rwxp 00000000 00:00 0                              [stack:16398] //not aligned to 2Mbytes
    Size:               8188 kB
    Rss:                   8 kB
    Pss:                   8 kB
    Shared_Clean:          0 kB
    Shared_Dirty:          0 kB
    Private_Clean:         0 kB
    Private_Dirty:         8 kB
    Referenced:            8 kB
    Anonymous:             8 kB
    AnonHugePages:         0 kB
    Swap:                  0 kB
    KernelPageSize:        4 kB
    MMUPageSize:           4 kB
    Locked:                0 kB
    VmFlags: rd wr ex mr mw me ac
```

THP improves the memory management efficiency a lot, there are benchmarks(https://lwn.net/Articles/423590/) 
showing improvements of up to 10% or so in some situations. 
So, it's a balance between more memory consumption and performance enhancement. 


### Investigation Notes:

TLB: A translation lookaside buffer
一个内存缓冲区，用于存储最近的虚拟内存到物理内存的映射，目的是提高效率。

linux kernel 使用hugetlb伪文件系统的实现方式提供大页面, hugetlbfs, support 2M memory allocation.

memory page default size x86: 4k

libhugetlbfs -> hugetlbfs

libhugetlbfs 对malloc()/free()等内存相关的函数做了重载。

Need config kernel before support hugetlbfs
```c
    CONFIG_HUGETLB_PAGE
    CONFIG_HUGETLBFS
```
Show number of huge pages in the system: `/proc/sys/vm/nr_hugepages`

Can set the number:
```c
    echo 20 > /proc/sys/vm/nr_hugepages
```
hugetlbfs 只能被mount起来才能使用。 
```c
    mount none /mnt/huge -t hugetlbfs
```
### Meaning of VSZ and RSS by `ps aux` command

**VSZ:**  virtual memory size of the process in KiB (1024-byte units).

**RSS: resident set size**, the non-swapped physical memory that a task has used (in kiloBytes).
Address space (ie. virtual memory in the process list) doesn't cost anything; it's not real. 
What's real is the RSS (RES) column, which is resident memory. 
That's how much of your actual memory a process is occupying(在内存中驻留的占用内存).

**但是还是有例外情况！**

But even that isn't the whole answer. If a process calls fork(), it splits into two parts, and both of them initially share all their RSS. 
So even if RSS was initially 1 GB, the result after forking would be two processes, each with an RSS of 1 GB, but you'd still only be using 1 GB of memory.

Confused yet? Here's what you really need to know: use the `free` command and check the results before and after starting your program (on the +/- buffers/cache line). **That difference is how much new memory your newly-started program used.**

### Transparent Huge pages

Huge Pages provide service by hugetlbfs, which is a filesystem. If a process want to use huge page, there is no method, so introduce the Transparent Hugepage.
Hugetlbfs is often seen as a feature for **large, proprietary database management systems and little else(主要用于数据库)**.
**But to use hugepages effectively, the kernel must find physically continuous areas of memory big enough to satisfy the request, and also properly aligned.**
For this, a `khugepaged kernel thread`has been added. This thread will occasionally attempt to substitute smaller pages being used currently with a hugepage allocation, 
thus maximizing THP usage.
```shell
    -bash-4.3# cat /sys/kernel/mm/transparent_hugepage/enabled 
    [always] madvise never
    -bash-4.3# cat /sys/kernel/mm/transparent_hugepage/defrag  
    [always] madvise never
    -bash-4.3#
```
always: enable in system wide

madvise: enable only when madvise is called

int madvise(void *addr, size_t length, int advice); //MADV_HUGEPAGE

Enables Transparent Huge Pages (THP) for pages in the range specified by addr and length. 
```shell
    -bash-4.3# cat /proc/meminfo |grep Anon
    AnonPages:         61936 kB
    AnonHugePages:     30720 kB
    -bash-4.3# 

    -bash-4.3# ps -ef |grep huge                         
    root       150     2  0  2012 ?        00:00:00 [khugepaged]
    root     14293 14223  0 21:34 pts/1    00:00:00 grep huge
    -bash-4.3# 
```
#### Show process with AnonHugePages used and their `ps` output
```shell
    grep -e AnonHugePages  /proc/*/smaps | awk  '{ if($2>4) print $0} ' |  awk -F "/"  '{print $0; system("ps -fp " $3)} '
```
bootargs:
```shell
    transparent_hugepage=madvise
```
KVM guests can be deployed with huge page memory support in order to improve performance by increasing CPU cache hits against the Transaction Lookaside Buffer (TLB). 

#### Sum all the HugePages used in the system:
```c
    -bash-4.3# grep AnonHugePages /proc/*/smaps | awk '{print $2}' |grep -v "^0" |  awk '{sum += $1;} END {print sum " KB" ;}'
    28672 KB
    -bash-4.3# cat /proc/meminfo |grep Huge
    AnonHugePages:     28672 kB
    HugePages_Total:       0
    HugePages_Free:        0
    HugePages_Rsvd:        0
    HugePages_Surp:        0
    Hugepagesize:       2048 kB
    -bash-4.3# 
```
### khugepaged
Since the Transparent HugePage patch was added to the 2.6.38 Kernel by Andrea Arcangeli. A new thread called khugepaged was introduced which scans for pages that could be merged into a single large page, once the pages have been merged into a single huge page, the smaller pages are removed and freed up.
If the huge page needs to be swapped to disk, then the single page is automatically split back up into smaller pages and written to disk, so THP basically is awesome. 


### Recommentations for embedded system

In certain cases when hugepages are enabled system wide, application
may end up allocating more memory resources. An application may mmap a
large region but only touch 1 byte of it, in that case a 2M page might
be allocated instead of a 4k page for no good. This is why it's
possible to disable hugepages system-wide and to only have them inside
MADV_HUGEPAGE madvise regions.

Embedded systems should enable hugepages only inside madvise regions
to eliminate any risk of wasting any precious byte of memory and to
only run faster.

Applications that gets a lot of benefit from hugepages and that don't
risk to lose memory by using hugepages, should use
madvise(MADV_HUGEPAGE) on their critical mmapped regions.

### Links related:

[Transparent huge pages in 2.6.38](https://lwn.net/Articles/423584/)

[Transparent Hugepage Support](https://lwn.net/Articles/423592/)

[similar problems from internet](http://www.linuxquestions.org/questions/linux-enterprise-47/transparent-huge-pages-usage-on-rhel-6-a-4175520796/)


### Set stack size when create a thread

I didn’t noticed that there is such procedure or porting guide line for 3rd party code about stack size, 
but I checked 3rd party code cpssd(from Marvell) which is under LFS maintaining, looks for some threads already changed the stack size to 32KB
```c
rc = cpssOsTaskCreate(apTaskName,               /* Task Name      */

                                                         100,                        /* Task Priority  */

                                                         _32K,                     /* Stack Size     */

                                                         lion2AppDemoApHndlTask,   /* Starting Point */

                                                         &apTaskParamsArr[portGroupId],/* Arguments list */

                                                         &apTaskId);               /* task ID        */
```

OOM Killer(内核如何知道哪个进程需要被杀死？)

code is in mm/oom_kill.c, The function `badness()` give a score to each existing
processes. The one with highest score will be the victim. The criteria are:



    VM size. This is not the sum of all allocated pages, but the sum of the size of all VMAs owned by the process. The bigger the VM size, the higher the score.
    Related to #1, the VM size of the process's children are important too. The VM size is cumulative if a process has one or more children.
    Processes with task priorities smaller than zero (niced processes) get more points.
    Superuser processes are important, by assumption; thus they have their scores reduced.
    Process runtime. The longer it runs, the lower the score.
    Processes that perform direct hardware access are more immune.
    The swapper (pid 0) and init (pid 1) processes, as well as any kernel threads immune from the list of potential victims.

http://www.linuxdevcenter.com/pub/a/linux/2006/11/30/linux-out-of-memory.html?page=2
-----------




