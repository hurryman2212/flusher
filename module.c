#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/version.h>

#include <asm/tlbflush.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(6, 4, 0)
#define class_create(owner, name) class_create(name)
#endif

static int dummy(void) { return -ENODEV; }
static void do_flush_cache(void) {
  get_cpu();
  wbinvd_on_all_cpus();
  put_cpu();
}
static void do_flush_tlb(void) { __flush_tlb_all(); }

static int flusher_open(struct inode *inode, struct file *file) {
  return dummy();
}
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = flusher_open,
};

static ssize_t flusher_sysfs_read(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf) {
  return dummy();
}

static ssize_t flusher_sysfs_write_cache(struct kobject *kobj,
                                         struct kobj_attribute *attr,
                                         const char *buf, size_t count) {
  int internal = 0;
  sscanf(buf, "%d", &internal);
  if (internal == 1) {
    do_flush_cache();
    return count;
  }
  return dummy();
}
static ssize_t flusher_sysfs_write_tlb(struct kobject *kobj,
                                       struct kobj_attribute *attr,
                                       const char *buf, size_t count) {
  int internal = 0;
  sscanf(buf, "%d", &internal);
  if (internal == 1) {
    do_flush_tlb();
    return count;
  }
  return dummy();
}

static dev_t dev;
static struct cdev cdev;
static struct class *class;
static struct device *device;
static struct kobject *kobj_cache, *kobj_tlb;
static struct kobj_attribute attr_cache =
                                 __ATTR(flush_cache, 0660, flusher_sysfs_read,
                                        flusher_sysfs_write_cache),
                             attr_tlb =
                                 __ATTR(flush_tlb, 0660, flusher_sysfs_read,
                                        flusher_sysfs_write_tlb);
static int __init flusher_init(void) {
  long ret;
  if (IS_ERR_VALUE(ret = alloc_chrdev_region(&dev, 0, 1, THIS_MODULE->name)))
    return ret;

  cdev_init(&cdev, &fops);

  if (IS_ERR_VALUE(ret = cdev_add(&cdev, dev, 1)))
    goto out_dev;

  if (IS_ERR(class = class_create(THIS_MODULE, THIS_MODULE->name))) {
    ret = PTR_ERR(class);
    goto out_cdev;
  }

  if (IS_ERR(device =
                 device_create(class, NULL, dev, NULL, THIS_MODULE->name))) {
    ret = PTR_ERR(device);
    goto out_class;
  }

  kobj_cache = kobject_create_and_add("cache", kernel_kobj);
  kobj_tlb = kobject_create_and_add("tlb", kernel_kobj);

  if (IS_ERR_VALUE(ret = sysfs_create_file(kobj_cache, &attr_cache.attr)))
    goto out_device;
  if (IS_ERR_VALUE(ret = sysfs_create_file(kobj_tlb, &attr_tlb.attr)))
    goto out_kobj_cache;

  return 0;
out_kobj_cache:
  kobject_put(kobj_cache);
  sysfs_remove_file(kernel_kobj, &attr_cache.attr);
out_device:
  device_destroy(class, dev);
out_class:
  class_destroy(class);
out_cdev:
  cdev_del(&cdev);
out_dev:
  unregister_chrdev_region(dev, 1);
  return (int)ret;
}
static void __exit flusher_exit(void) {
  kobject_put(kobj_tlb);
  sysfs_remove_file(kernel_kobj, &attr_tlb.attr);

  kobject_put(kobj_cache);
  sysfs_remove_file(kernel_kobj, &attr_cache.attr);

  device_destroy(class, dev);

  class_destroy(class);

  cdev_del(&cdev);

  unregister_chrdev_region(dev, 1);
}
module_init(flusher_init);
module_exit(flusher_exit);

MODULE_LICENSE("GPL v2");
