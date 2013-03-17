/* Copyright (c) 2012, Will Tisdale <willtisdale@gmail.com>. All rights reserved.
 *
 * 2012 Enhanced by motley <motley.slate@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifdef CONFIG_SUSPEND

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/earlysuspend.h>

#define CPUS_AVAILABLE		num_possible_cpus()

struct delayed_work hotplug_online_all_work;
struct delayed_work hotplug_offline_all_work;

bool single_core_mode = false;

static void __cpuinit hotplug_online_all_work_fn(struct work_struct *work)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		if (likely(!cpu_online(cpu))) {
			cpu_up(cpu);
		}
	}
}

static void __cpuinit hotplug_offline_all_work_fn(struct work_struct *work)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		if (likely(cpu_online(cpu) && (cpu))) {
			cpu_down(cpu);
		}
	}
}

static void kowalski_hp_resume(struct early_suspend *handler){
	if (!single_core_mode) {
		schedule_delayed_work_on(0, &hotplug_online_all_work, HZ);
	}
}

static void kowalski_hp_suspend(struct early_suspend *handler) {
	if (num_online_cpus() > 1) {
		schedule_delayed_work_on(0, &hotplug_offline_all_work, HZ);
	}
}

static struct early_suspend kowalski_hp_suspend_handler = {
	.suspend = kowalski_hp_suspend,
	.resume = kowalski_hp_resume,
};

static ssize_t single_core_mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", (single_core_mode ? 1 : 0));
}

static ssize_t single_core_mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long data = simple_strtoul(buf, NULL, 10);
	if(data) {
		if (! single_core_mode) {
			single_core_mode = true;
			schedule_delayed_work_on(0, &hotplug_offline_all_work, HZ);
		}
	} else {
		if (single_core_mode) {
			single_core_mode = false;
			schedule_delayed_work_on(0, &hotplug_online_all_work, HZ);
		}
	}
	return count;
}

static struct kobj_attribute single_core_mode_attribute =
	__ATTR(single_core_mode, 0666, single_core_mode_show, single_core_mode_store);

static struct attribute *auto_hotplug_attrs[] = {
	&single_core_mode_attribute.attr,
	NULL,
};

static struct attribute_group auto_hotplug_attr_group = {
	.attrs = auto_hotplug_attrs,
};

static struct kobject *auto_hotplug_kobj;

int __init auto_hotplug_init(void)
{
	int res;

	pr_info("auto_hotplug: v0.220 by _thalamus\n");
	pr_info("auto_hotplug: rev 1 enhanced by motley\n");
	pr_info("auto_hotplug: rev 1.3 modified by pengus77\n");
	pr_info("auto_hotplug: %d CPUs detected\n", CPUS_AVAILABLE);

	INIT_DELAYED_WORK_DEFERRABLE(&hotplug_online_all_work, hotplug_online_all_work_fn);
	INIT_DELAYED_WORK_DEFERRABLE(&hotplug_offline_all_work, hotplug_offline_all_work_fn);

	register_early_suspend(&kowalski_hp_suspend_handler);

	auto_hotplug_kobj = kobject_create_and_add("auto_hotplug", kernel_kobj);
	if (!auto_hotplug_kobj) {
		pr_err("%s auto_hotplug kobject create failed!\n", __FUNCTION__);
		return -ENOMEM;
	}

	res = sysfs_create_group(auto_hotplug_kobj, &auto_hotplug_attr_group);

	if (res) {
		pr_info("%s auto_hotplug sysfs create failed!\n", __FUNCTION__);
		kobject_put(auto_hotplug_kobj);
	}

	return res;
}
late_initcall(auto_hotplug_init);

#endif /* CONFIG_SUSPEND */
