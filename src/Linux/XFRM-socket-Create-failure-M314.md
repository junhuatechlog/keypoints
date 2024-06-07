# M314
----
1. [Mansho Descritpion](#mansho-314-description)
1. [Analysis](#analysis)
1. [Function call analysis](#calling-socket-function)
1. [xfrm_user_init procedure](#xfrm_user_init-procedure)
1. [Zhou Libing Analysis](#zhou-libing-analysis-based-on-debug-print)
1. [Idea get from David Meng](#idea-get-from-david-meng)

## Mansho 314 Description

When trs call socket() to create a XFRM netlink, it return error number 120, means 'Protocol not supported'.

**Error logs:**

    1011-0-CCS <2013-01-01T00:00:47.224434Z> ECB-LinuxListener ERR/LFS/LinuxSyslog,      trsXfrmBridge[5369]:  XFRM: XfrmBridgeMessageDispatcher::isConnected() socket creation failed. errno=120 = 'Protocol not supported'
    1011-0-CCS <2013-01-01T00:00:47.362647Z> ECB-LinuxListener INF/LFS/LinuxSyslog,      kernel: [   66.117384] Initializing XFRM netlink socket

## Analysis

Found the xfrm_user.ko/xfrm_algo.ko are loaded when the socket() with NETLINK_XFRM called, 
"Initializing XFRM netlink socket" will be printed when the xfrm_user_init() is executed. 

We suspect there is some race condition when the socket() function is called, maybe 2 process call socket() paralely? 
But now, don't have time to find RC, trs need us to build the kernel module in the kernel to avoid the problem. 

Test Env: 

Initializing XFRM netlink socket.
 10.68.248.83; test/test2008

[junhuawa@hzling40]$cat lcpa.config |grep XFRM

    CONFIG_XFRM=y
    CONFIG_XFRM_ALGO=m
    CONFIG_XFRM_USER=m
    # CONFIG_XFRM_SUB_POLICY is not set
    # CONFIG_XFRM_MIGRATE is not set
    CONFIG_XFRM_STATISTICS=y
    CONFIG_XFRM_IPCOMP=m
    CONFIG_INET_XFRM_TUNNEL=m
    CONFIG_INET_XFRM_MODE_TRANSPORT=m
    CONFIG_INET_XFRM_MODE_TUNNEL=m
    CONFIG_INET_XFRM_MODE_BEET=m
    CONFIG_INET6_XFRM_TUNNEL=m
    CONFIG_INET6_XFRM_MODE_TRANSPORT=m
    CONFIG_INET6_XFRM_MODE_TUNNEL=m
    CONFIG_INET6_XFRM_MODE_BEET=m
    # CONFIG_INET6_XFRM_MODE_ROUTEOPTIMIZATION is not set
    # CONFIG_SECURITY_NETWORK_XFRM is not set

Board's kernel configuration are stored in dir /var/fpwork1/junhuawa/XXX/src-bos/src/kernel/configs.

Log from my Redhat PC:

    [    2.160267] usbcore: registered new interface driver usbhid
    [    2.160269] usbhid: USB HID core driver
    [    2.160328] drop_monitor: Initializing network drop monitor service
    [    2.160485] TCP: cubic registered
    [    2.160495] Initializing XFRM netlink socket
    [    2.160655] NET: Registered protocol family 10
    [    2.161060] NET: Registered protocol family 17
    [    2.161644] Loading compiled-in X.509 certificates
    [    2.163061] Loaded X.509 cert 'Red Hat Enterprise Linux Driver Update Program (key 3): bf57f3e87362bc7229d9f465321773dfd1f77a80'
    [    2.164414] Loaded X.509 cert 'Red Hat Enterprise Linux kpatch signing key: 4d38fd864ebe18c5f0b72e3852e2014c3a676fc8'
    [    2.165771] Loaded X.509 cert 'Red Hat Enterprise Linux kernel signing key: 20a9713c3a76dc805fca64027c48c34de8fae907'
    [    2.165818] registered taskstats version 1

[junhuawa@Tesla ~]$ cat /boot/config-3.10.0-327.el7.x86_64 |grep XFRM

    CONFIG_XFRM=y
    CONFIG_XFRM_ALGO=y
    CONFIG_XFRM_USER=y
    CONFIG_XFRM_SUB_POLICY=y
    CONFIG_XFRM_MIGRATE=y
    CONFIG_XFRM_STATISTICS=y
    CONFIG_XFRM_IPCOMP=m
    CONFIG_INET_XFRM_TUNNEL=m
    CONFIG_INET_XFRM_MODE_TRANSPORT=m
    CONFIG_INET_XFRM_MODE_TUNNEL=m
    CONFIG_INET_XFRM_MODE_BEET=m
    CONFIG_INET6_XFRM_TUNNEL=m
    CONFIG_INET6_XFRM_MODE_TRANSPORT=m
    CONFIG_INET6_XFRM_MODE_TUNNEL=m
    CONFIG_INET6_XFRM_MODE_BEET=m
    CONFIG_INET6_XFRM_MODE_ROUTEOPTIMIZATION=m
    CONFIG_SECURITY_NETWORK_XFRM=y

The xfrm_user/algo already built in kernel.

Map between kernel module names and systematic designations:
    
    /lib/modules/$VERSION/modules.alias

sh-4.3# cat /lib/modules/3.10.64--fblrclcplfs15120061-lcpa/modules.alias  |more

    # Aliases extracted from modules themselves.
    alias nfs-layouttype4-1 nfs_layout_nfsv41_files
    alias fs-cramfs cramfs
    alias fs-nfsd nfsd
    alias devname:fuse fuse
    alias char-major-10-229 fuse
    alias fs-fuseblk fuse
    alias fs-fuse fuse
    alias fs-fusectl fuse
    alias fs-pramfs pramfs
    alias cipher_null crypto_null
    alias digest_null crypto_null
    alias compress_null crypto_null
    alias sha1 sha1_generic
    alias sha256 sha256_generic
    alias sha224 sha256_generic
    alias des des_generic
    alias des3_ede des_generic
    alias blowfish blowfish_generic
    alias stdrng ansi_cprng
    alias i2c:tca6424 gpio_pca953x
    alias i2c:tca6416 gpio_pca953x
    alias i2c:tca6408 gpio_pca953x
    ......

Builtin kernel modules list:

    /lib/modules/$VERSION/modules.builtin

[junhuawa@Tesla pronto]$ cat /lib/modules/3.10.0-327.22.2.el7.x86_64/modules.builtin|grep xfrm

    kernel/net/xfrm/xfrm_algo.ko
    kernel/net/xfrm/xfrm_user.ko


Create socket link, and call is_connect() to check if link is ok, return "protocol is not supported!".
Check when the xfrm_user.ko kernel module is loaded since it's not compiled into the kernel. 

## Calling socket function

    s = socket(AF_NETLINK, SOCK_RAW, NETLINK_XFRM);
    #define EPROTONOSUPPORT 120 /* Protocol not supported */

Call socket() will load the required kernel module automatically: 

/net/socket.c
```c
int sock_create(int family, int type, int protocol, struct socket **res)
{
    return __sock_create(current->nsproxy->net_ns, family, type, protocol, res, 0);
}
```
```c
int __sock_create(struct net *net, int family, int type, int protocol,
    struct socket **res, int kern)
```

```c
    /* Now protected by module ref count */
    rcu_read_unlock();

    err = pf->create(net, sock, protocol, kern);
    if (err < 0)
        goto out_module_put;

```
net/netlink/af_netlink.c

static int netlink_create(struct net *net, struct socket *sock, int protocol,
              int kern)
```c
    netlink_lock_table();
#ifdef CONFIG_MODULES
    if (!nl_table[protocol].registered) {
        netlink_unlock_table();
        request_module("net-pf-%d-proto-%d", PF_NETLINK, protocol);
        netlink_lock_table();
    }   
#endif
    if (nl_table[protocol].registered &&
        try_module_get(nl_table[protocol].module))
        module = nl_table[protocol].module;
    else
        err = -EPROTONOSUPPORT;
```
It will check the nl_table[] if the protocol have been registered, 
if not, it will install the net-pf-16-proto-6 kernel module(xfrm_user.ko).
If the protocol is still not registered, error code 120 will be returned.

```c
static const struct net_proto_family netlink_family_ops = {
    .family = PF_NETLINK,
    .create = netlink_create,
    .owner  = THIS_MODULE,  /* for consistency 8) */
};
```
--> kernel/kmod.c

    request_module("net-pf-%d", family);

/include/linux/kmod.h

    #define request_module(mod...) __request_module(true, mod)

--> 
ret = call_modprobe(module_name, wait ? UMH_WAIT_PROC : UMH_WAIT_EXEC);

-> call_usermodehelper_setup() to initialize the workqueue

-> call_usermodehelper_exec() to wait until the user process(modprobe) execute complete!


### xfrm_user_init procedure

    xfrm_user_init -> 
    register_pernet_subsys(&xfrm_user_net_ops) // in net/core/net_namespace.c
    -> register_pernet_operations(first_device, ops) 
    -> __register_pernet_operations(list, ops)
    -> ops_init
    -> ops->init(net) <--> xfrm_user_net_init(struct net *net)
    -> netlink_kernel_create(net, NETLINK_XFRM, &cfg)
    -> __netlink_kernel_create() in net/netlink/af_netlink.c
    -> it will set the nl_table[unit].registered to 1. 

## ZhouLB Analysis based on debug print

Add debug print process name/pid in socket() interface, found there are at least 2 threads/process call the socket() to create XFRM netlink in parallel. 
    
    [   77.930186] ++++++ xfrm_user_net_init1 by pid:6418, task:modprobe+++++     +^M
    [   77.937287] ++++++ xfrm_user_net_init done by pid:6418, task:modprobe+++++     +^M
    [   77.945354] ++++++ request_module done:netlink_create by pid:5409, task:trsKeyRetrieve++++++^M
    [   77.945427] ++++++ request_module done:netlink_create by pid:5373, task:trsXfrmBridge++++++^M
    [   77.946869] ++++++ xfrm_user_net_init1 by pid:6419, task:vsftpd+++++     +^M
    [   77.946881] ++++++ xfrm_user_net_init done by pid:6419, task:vsftpd+++++     +^M

### Based on this found, he write a simple program to test create socket by 3 threads in parallel. 

By this [test program](../test_tools/socket_xfrm.tar.gz), the problem can be reproduced easily. 

### Debugging in the linux kernel

By debugging in the kernel code, we found when 3 threads call socket() to let it 
call userspace process modprobe to load the kernel module. 
3 threads will wait until the initialized work be complete. 

But sometimes, after the register_pernet_subsys() call end, 1-2 threads will return from wait immediately, the request_module() call end, it check if the protocol registered, found not, 
so, errno 120 is return. 

## Idea get from DavidM  

He have known there is a race condition in the kmod source code, and has been patched in the latest kmod version. 

The OS can reproduce the case have kmod version 18, but in my Redhat/CentOS7, can't reproduce the case because kmod version is 20. 

From [the patch](../assets/linux/0001-Fix-race-while-loading-modules.patch) got from kmod git repo, it already show there exist race condition in kmod-18 and older versions.

Use kmod version 21 in product, test 10000 times, can't reproduce the problem. 

[kmod source code commit of the patch](https://github.com/lucasdemarchi/kmod/commit/fd44a98ae2eb5eb32161088954ab21e58e19dfc4)

NETLINK      

    Kernel-user communication protocol.

Supported Address families, defined in /linux/socket.h
AF_NETLINK: 

    Communication between kernel and user space. Datagram-oriented service. 

AF_UNIX: 

    For local inter-porcess communication


PF_NETLINK: Protocol families, same as address families.

register_pernet_subsys - register a network namespace subsystem
