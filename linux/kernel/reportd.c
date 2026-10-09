/* kernel/reportd.c - Report Daemon supporter. */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/kfifo.h>
#include <linux/sysfs.h>
#include <linux/pid.h>
#include <trace/events/syscalls.h>
#include <trace/events/printk.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tr1zz");
MODULE_DESCRIPTION("Report daemon supporter module.");
#define DEVICE_NAME "reportd"
#define CLASS_NAME "reportd"
#define FIFO_SIZE (128 * 1024)
static STRUCT_KFIFO(char, FIFO_SIZE) reportd_fifo;
static DEFINE_SPINLOCK(audit_lock);
static int major_num;
static struct device *reportd_device = NULL;
static struct pid *owner_pid = NULL;
static DEFINE_MUTEX(owner_mutex);
static atomic_long_t dropped_bytes = ATOMIC_LONG_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(audit_wait_queue);
static ssize_t dropped_bytes_show(const struct class *class, const struct class_attribute *attr, char *buf) {
    return sysfs_emit(buf, "%ld\n", atomic_long_read(&dropped_bytes));
}
static CLASS_ATTR_RO(dropped_bytes);
static struct attribute *reportd_class_attrs[] = {
    &class_attr_dropped_bytes.attr,
    NULL,
};
ATTRIBUTE_GROUPS(reportd_class);
static struct class reportd_class = {
    .name = CLASS_NAME,
    .class_groups = reportd_class_groups,
};
void reportd_log_event(const char *msg, size_t len) {
    unsigned long flags;
    unsigned int copied;
    if (!msg || len == 0)
        return;
    spin_lock_irqsave(&audit_lock, flags);
    copied = kfifo_in(&reportd_fifo, msg, len);
    spin_unlock_irqrestore(&audit_lock, flags);
    if (unlikely(copied < len))
        atomic_long_add(len - copied, &dropped_bytes);
    if (copied > 0)
        wake_up_interruptible(&audit_wait_queue);
}
EXPORT_SYMBOL(reportd_log_event);
static void probe_sys_enter(void *data, struct pt_regs *regs, long id) {
    char buf[128];
    int len;
    if (task_pid_nr(current) == 1)
        return;
    len = snprintf(buf, sizeof(buf), "[SYSCALL] PID:%d COMM:%s SYS:%ld\n", task_pid_nr(current), current->comm, id);
    if (len > 0)
        reportd_log_event(buf, len);
}
static void probe_console_printk(void *data, const char *text, size_t len) {
    char buf[256];
    int formatted_len = snprintf(buf, sizeof(buf), "[KERN_PRINTK] %.*s", (int)len, text);
    if (formatted_len > 0)
        reportd_log_event(buf, formatted_len);
}
static int register_event_tracers(void) {
    int ret = register_trace_sys_enter(probe_sys_enter, NULL);
    if (ret) {
        pr_err("reportd: error while registring kallsysm tracepoint (%d)\n", ret);
        return ret;
    }
    ret = register_trace_console(probe_console_printk, NULL);
    if (ret)
        pr_warn("reportd: error registring kern_con/printk (%d)\n", ret);
    return 0;
}
static ssize_t audit_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char kbuf[256];
    size_t copy_len;
    if (unlikely(file->private_data != owner_pid || task_pid(current) != owner_pid))
        return -EPERM;
    if (count == 0)
        return 0;
    copy_len = min(count, sizeof(kbuf) - 1);
    if (copy_from_user(kbuf, buf, copy_len))
        return -EFAULT;
    kbuf[copy_len] = '\0';
    reportd_log_event(kbuf, copy_len);
    return count;
}
static void unregister_event_tracers(void) {
    unregister_trace_sys_enter(probe_sys_enter, NULL);
    unregister_trace_console(probe_console_printk, NULL);
}
static int audit_open(struct inode *inode, struct file *file) {
    struct task_struct *task;
    struct pid *cur_pid = task_pid(current);
    if (task_tgid_nr_ns(current, &init_pid_ns) != 1) {
        pr_warn("reportd: denied access to %d (is not pid 1)\n", task_pid_nr(current));
        return -EPERM;
    }
    mutex_lock(&owner_mutex);
    if (owner_pid) {
        task = pid_task(owner_pid, PIDTYPE_PID);
        if (task && !(task->flags & PF_EXITING)) {
            mutex_unlock(&owner_mutex);
            return -EBUSY;
        }
        put_pid(owner_pid);
        owner_pid = NULL;
    }
    owner_pid = get_pid(cur_pid);
    file->private_data = get_pid(cur_pid);
    mutex_unlock(&owner_mutex);
    return 0;
}
static int audit_release(struct inode *inode, struct file *file) {
    struct pid *f_pid = file->private_data;
    mutex_lock(&owner_mutex);
    if (owner_pid && owner_pid == f_pid) {
        put_pid(owner_pid);
        owner_pid = NULL;
    }
    mutex_unlock(&owner_mutex);
    if (f_pid) {
        put_pid(f_pid);
        file->private_data = NULL;
    }
    return 0;
}
static ssize_t audit_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    unsigned int copied;
    int ret;
    if (unlikely(file->private_data != owner_pid || task_pid(current) != owner_pid))
        return -EPERM;
    if (count == 0)
        return 0;
    if (kfifo_is_empty(&reportd_fifo) && (file->f_flags & O_NONBLOCK))
        return -EAGAIN;
    if (wait_event_interruptible(audit_wait_queue, !kfifo_is_empty(&reportd_fifo)))
        return -ERESTARTSYS;
    ret = kfifo_to_user(&reportd_fifo, buf, count, &copied);
    if (ret)
        return ret;
    return copied;
}
static __poll_t audit_poll(struct file *file, poll_table *wait) {
    __poll_t mask = 0;
    if (unlikely(file->private_data != owner_pid || task_pid(current) != owner_pid))
        return EPOLLERR;
    poll_wait(file, &audit_wait_queue, wait);
    if (!kfifo_is_empty(&reportd_fifo))
        mask |= EPOLLIN | EPOLLRDNORM;
    return mask;
}
static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = audit_open,
    .release = audit_release,
    .read    = audit_read,
    .poll    = audit_poll,
    .write   = audit_write,
};
static int __init reportd_init(void) {
    int ret;
    INIT_KFIFO(reportd_fifo);
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0)
        return major_num;
    ret = class_register(&reportd_class);
    if (ret) {
        pr_err("reportd: can't make sysfs class (%d)\n", ret);
        goto err_unregister_chrdev;
    }
    reportd_device = device_create(&reportd_class, NULL, MKDEV(major_num, 0), NULL, DEVICE_NAME);
    if (IS_ERR(reportd_device)) {
        ret = PTR_ERR(reportd_device);
        goto err_unregister_class;
    }
    ret = register_event_tracers();
    if (ret)
        goto err_destroy_device;
    pr_info("reportd: character device at /dev/%s \n", DEVICE_NAME);
    pr_info("reportd: (im not spoofing your life btw...)\n");
    pr_info("reportd: kernel: at the moment safe and healty :)\n");
    return 0;
err_destroy_device:
    device_destroy(&reportd_class, MKDEV(major_num, 0));
err_unregister_class:
    class_unregister(&reportd_class);
err_unregister_chrdev:
    unregister_chrdev(major_num, DEVICE_NAME);
    return ret;
}
static void __exit reportd_exit(void) {
    unregister_event_tracers();
    device_destroy(&reportd_class, MKDEV(major_num, 0));
    class_unregister(&reportd_class);
    unregister_chrdev(major_num, DEVICE_NAME);
    mutex_lock(&owner_mutex);
    if (owner_pid) {
        put_pid(owner_pid);
        owner_pid = NULL;
    }
    mutex_unlock(&owner_mutex);
}
module_init(reportd_init);
module_exit(reportd_exit);

