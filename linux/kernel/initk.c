#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/pid_namespace.h>
#include <linux/uaccess.h>

#define INITK_MAX_MSG 256

SYSCALL_DEFINE2(syskp, const char __user *, msg, size_t, len)
{
	char kbuf[INITK_MAX_MSG];
	if (task_tgid_nr_ns(current, &init_pid_ns) != 1)
		return -EPERM;
	if (!msg || len == 0)
		return -EINVAL;
	if (len >= INITK_MAX_MSG)
		len = INITK_MAX_MSG - 1;
	if (copy_from_user(kbuf, msg, len))
		return -EFAULT;
	kbuf[len] = '\0';
	printk("%s\n", kbuf);
	return 0;
}

