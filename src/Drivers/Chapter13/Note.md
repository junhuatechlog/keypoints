### kobject kobj_type and kset

Linux device model 依赖3个重要的结构: kobject, kobj_type, kset. 

kobject 是设备模型的核心， 在后台运行， 它为内核带来类似于OO的编程风格，主要用于引用计数以及提供设备层次和它们之间的关系。 

```c
include/linux/kobject.h

struct kobject {
        const char              *name;
        struct list_head        entry;
        struct kobject          *parent;//用于生成sys下的目录结构
        struct kset             *kset;//这个对象属于哪组对象
        const struct kobj_type  *ktype;//要加入的对象的指针
        struct kernfs_node      *sd; /* sysfs directory entry */
        struct kref             kref;//引用计数
```

可以用kobject_init(kobj, ktype)将kobj_type作为参数传递给kobject.
kobject_add可以指定parent kobject, 如果为NULL, 则将其父项设置为kset, 对象将成为顶级sys目录的自成员. 


struct kobj_type描述kobject的行为.它将控制在创建和销毁kobject时，以及在读取或写入属性时发生的操作，该操作由sysfs_ops定义。 

```c
struct kobj_type {
        void (*release)(struct kobject *kobj);
        const struct sysfs_ops *sysfs_ops;
        const struct attribute_group **default_groups;
        const struct kobj_ns_type_operations *(*child_ns_type)(const struct kobject *kobj);
        const void *(*namespace)(const struct kobject *kobj);
        void (*get_ownership)(const struct kobject *kobj, kuid_t *uid, kgid_t *gid);
};

```
```c
struct sysfs_ops {
        ssize_t (*show)(struct kobject *, struct attribute *, char *);
        ssize_t (*store)(struct kobject *, struct attribute *, const char *, size_t);
};
```

kset是一个kobject的集合(链表), 将相关的kobject收集到一个位置.
```c
struct kset {
        struct list_head list;//相同类型对象的链表
        spinlock_t list_lock;
        struct kobject kobj;//该集合的基类
        const struct kset_uevent_ops *uevent_ops;
} __randomize_layout;
```

### 属性
attributes是由kobject到出到用户空间的sysfs文件， 属性表示可以从用户空间读取，写入或同时具有这两者的对象属性。 属性将内核数据映射到sysfs中的文件. 

```c
struct attribute {
        const char              *name;
        umode_t                 mode;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
        bool                    ignore_lockdep:1;
        struct lock_class_key   *key;
        struct lock_class_key   skey;
#endif
};
struct attribute_group {
        const char              *name;
        umode_t                 (*is_visible)(struct kobject *,
                                              struct attribute *, int);
        umode_t                 (*is_bin_visible)(struct kobject *,
                                                  struct bin_attribute *, int);
        struct attribute        **attrs;
        struct bin_attribute    **bin_attrs;
};
```
类似的属性可以组成属性组，通过sysfs_create_group()挂到sysfs目录下面。 

```c
int __must_check sysfs_create_group(struct kobject *kobj,
                                    const struct attribute_group *grp);
```

### sysfs
sysfs是持久虚拟文件系统，提供系统的全局视图，并通过kobject显示内核对象的层次结构(拓扑). 每个kobject显示为目录和目录中的文件。 
下面顶级sysfs目录对应的kobject, 其他kobject可以挂接在这些根节点下面:

```c
kernel_kobj,
power_kobj,
firmware_kobj,//drivers/base/firmware.c
hypervisor_kobj, //drivers/base/hypervisor.c
fs_kobj, //fs/namespace.c
```

顶级sysfs目录还有其他的kobjet, 是在各个子系统中注册:
class/, dev/, devices/ 在drivers/base/core.c中的devices_init函数创建. 
block在block/genhd.c中创建。
bus在drivers/base/bus.c中被创建为kset.

```c
	block/genhd.c
       /* create top-level block dir */
        block_depr = kobject_create_and_add("block", NULL); 

	drivers/base/bus.c
        bus_kset = kset_create_and_add("bus", &bus_uevent_ops, NULL);
        if (!bus_kset)
                return -ENOMEM;

        system_kset = kset_create_and_add("system", NULL, &devices_kset->kobj);

```

