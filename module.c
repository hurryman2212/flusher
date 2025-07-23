#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/version.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(6, 4, 0)
#define class_create(owner, name) class_create(name)
#endif

#if defined(__x86_64__) || defined(__i386__)
static void do_flush_ept(void *dummy) {
  struct {
    uint64_t eptp, reserved;
  } __attribute__((packed)) descriptor = {0};
  uint64_t type = 2;

  asm volatile("invept %[descriptor], %[type]"
               :
               : [descriptor] "m"(descriptor), [type] "r"(type)
               : "cc", "memory");
}

static inline void amd_invlpga_all(void) {
  asm volatile("invlpga %[addr], %[asid]"
               :
               : [addr] "a"(0ULL), [asid] "c"(0)
               : "memory");
}
static void do_flush_npt(void *dummy) {
  u64 efer;
  rdmsrl(MSR_EFER, efer);
  if (!(efer & EFER_SVME))
    wrmsrl(MSR_EFER, efer | EFER_SVME);

  amd_invlpga_all();
}
#endif

static void do_flush_cache(void *dummy) {
#if defined(__x86_64__) || defined(__i486__)
  wbinvd();
#else
  flush_cache_all();
#endif
}

static void do_flush_tlb(void *dummy) { __flush_tlb_all(); }

static int flusher_open(struct inode *inode, struct file *file) {
  return -ENODEV;
}
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = flusher_open,
};

static struct kobject *kobj_cache,
#if defined(__x86_64__) || defined(__i386__)
    *kobj_ept = NULL, *kobj_npt = NULL,
#endif
    *kobj_tlb;
static ssize_t flusher_sysfs_read_invalid(struct kobject *kobj,
                                          struct kobj_attribute *attr,
                                          char *buf) {
  return -ENODEV;
}
static ssize_t flusher_sysfs_write(struct kobject *kobj,
                                   struct kobj_attribute *attr, const char *buf,
                                   size_t count) {
  if (kobj == kobj_cache) {
    on_each_cpu(do_flush_cache, NULL, 1);
    printk(KERN_INFO "Flushing cache!\n");
  }
#if defined(__x86_64__) || defined(__i386__)
  else if (kobj == kobj_ept) {
    on_each_cpu(do_flush_ept, NULL, 1);
    printk(KERN_INFO "Flushing EPT TLB!\n");
  } else if (kobj == kobj_npt) {
    on_each_cpu(do_flush_npt, NULL, 1);
    printk(KERN_INFO "Flushing NPT TLB!\n");
  }
#endif
  else if (kobj == kobj_tlb) {
    on_each_cpu(do_flush_tlb, NULL, 1);
    printk(KERN_INFO "Flushing TLB!\n");
  } else
    return -ENODEV;
  return count;
}
static struct kobj_attribute flusher_attr =
    __ATTR(flush, 0660, flusher_sysfs_read_invalid, flusher_sysfs_write);

static dev_t dev;
static struct cdev cdev;
static struct class *class;
static struct device *device;

enum out_layer {
  out_dev,
  out_cdev,
  out_class,
  out_device,
  out_kobj_cache,
  out_kobj_ept,
  out_kobj_npt,
  out_kobj_tlb,
  out_sysfs_cache,
  out_sysfs_npt,
  out_sysfs_tlb,
  out_full
};
static void flusher_cleanup(enum out_layer layer) {
  switch (layer) {
  case out_full:
    fallthrough;
  case out_sysfs_tlb:
    sysfs_remove_file(kobj_tlb, &flusher_attr.attr);
    fallthrough;
  case out_sysfs_npt:
    if (kobj_npt)
      sysfs_remove_file(kobj_npt, &flusher_attr.attr);
    fallthrough;
  case out_sysfs_cache:
    sysfs_remove_file(kobj_cache, &flusher_attr.attr);
    fallthrough;
  case out_kobj_tlb:
    kobject_del(kobj_tlb);
    kobject_put(kobj_tlb);
    fallthrough;
  case out_kobj_ept:
    if (kobj_ept) {
      kobject_del(kobj_ept);
      kobject_put(kobj_ept);
    }
    fallthrough;
  case out_kobj_npt:
    if (kobj_npt) {
      kobject_del(kobj_npt);
      kobject_put(kobj_npt);
    }
    fallthrough;
  case out_kobj_cache:
    kobject_del(kobj_cache);
    kobject_put(kobj_cache);
    fallthrough;
  case out_device:
    device_destroy(class, dev);
    fallthrough;
  case out_class:
    class_destroy(class);
    fallthrough;
  case out_cdev:
    cdev_del(&cdev);
    fallthrough;
  case out_dev:
    unregister_chrdev_region(dev, 1);
  }
}

static int __init flusher_init(void) {
  long ret;
  long long ept_vpid_cap;

  if (IS_ERR_VALUE(ret = alloc_chrdev_region(&dev, 0, 1, THIS_MODULE->name)))
    return ret;

  cdev_init(&cdev, &fops);
  if (IS_ERR_VALUE(ret = cdev_add(&cdev, dev, 1))) {
    flusher_cleanup(out_dev);
    return ret;
  }

  if (IS_ERR(class = class_create(THIS_MODULE, THIS_MODULE->name))) {
    flusher_cleanup(out_cdev);
    return PTR_ERR(class);
  }

  if (IS_ERR(device =
                 device_create(class, NULL, dev, NULL, THIS_MODULE->name))) {
    flusher_cleanup(out_class);
    return PTR_ERR(device);
  }

  if (!(kobj_cache = kobject_create_and_add("cache", kernel_kobj))) {
    flusher_cleanup(out_device);
    return -ENOMEM;
  }
#if defined(__x86_64__) || defined(__i386__)
  rdmsrl_safe(MSR_IA32_VMX_EPT_VPID_CAP, &ept_vpid_cap);
  if (ept_vpid_cap & (1 << 26) &&
      !(kobj_ept = kobject_create_and_add("ept", kernel_kobj))) {
    flusher_cleanup(out_kobj_cache);
    return -ENOMEM;
  }
  if (boot_cpu_has(X86_FEATURE_SVM) && boot_cpu_has(X86_FEATURE_NPT) &&
      !(kobj_npt = kobject_create_and_add("npt", kernel_kobj))) {
    flusher_cleanup(out_kobj_ept);
    return -ENOMEM;
  }
#endif
  if (!(kobj_tlb = kobject_create_and_add("tlb", kernel_kobj))) {
    flusher_cleanup(out_kobj_npt);
    return -ENOMEM;
  }

  if (IS_ERR_VALUE(ret = sysfs_create_file(kobj_cache, &flusher_attr.attr))) {
    flusher_cleanup(out_kobj_tlb);
    return ret;
  }
  if (kobj_ept &&
      IS_ERR_VALUE(ret = sysfs_create_file(kobj_ept, &flusher_attr.attr))) {
    flusher_cleanup(out_sysfs_cache);
    return ret;
  }
  if (kobj_npt &&
      IS_ERR_VALUE(ret = sysfs_create_file(kobj_npt, &flusher_attr.attr))) {
    flusher_cleanup(out_sysfs_cache);
    return ret;
  }
  if (IS_ERR_VALUE(ret = sysfs_create_file(kobj_tlb, &flusher_attr.attr))) {
    flusher_cleanup(out_sysfs_npt);
    return ret;
  }

  return 0;
}
static void __exit flusher_exit(void) { flusher_cleanup(out_full); }
module_init(flusher_init);
module_exit(flusher_exit);

MODULE_LICENSE("GPL v2");
