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

/*
 * Generic auto hotplug driver for ARM SoCs. Targeted at current generation
 * SoCs with dual and quad core applications processors.
 * Automatically hotplugs online and offline CPUs based on system load.
 * It is also capable of immediately onlining a core based on an external
 * event by calling void hotplug_boostpulse(void)
 *
 * Not recommended for use with OMAP4460 due to the potential for lockups
 * whilst hotplugging.
 */

#ifdef CONFIG_HAS_EARLYSUSPEND

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/earlysuspend.h>

#define CPUS_AVAILABLE		num_possible_cpus()

/* Control flags */
unsigned char flags;
#define EARLYSUSPEND_ACTIVE	(1 << 0)

struct delayed_work hotplug_online_all_work;
struct delayed_work hotplug_offline_all_work;

static void __cpuinit hotplug_online_all_work_fn(struct work_struct *work)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		if (likely(!cpu_online(cpu))) {
			cpu_up(cpu);
		}
	}
}

static void hotplug_offline_all_work_fn(struct work_struct *work)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		if (likely(cpu_online(cpu) && (cpu))) {
			cpu_down(cpu);
		}
	}
}

static void auto_hotplug_early_suspend(struct early_suspend *handler)
{
	if (!(flags & EARLYSUSPEND_ACTIVE)) {
		if (num_online_cpus() > 1) {
			schedule_delayed_work_on(0, &hotplug_offline_all_work, HZ);
			flags |= EARLYSUSPEND_ACTIVE;
		}
	}
}

static void auto_hotplug_late_resume(struct early_suspend *handler)
{
	if (flags & EARLYSUSPEND_ACTIVE) {
		schedule_delayed_work_on(0, &hotplug_online_all_work, HZ);
		flags &= ~EARLYSUSPEND_ACTIVE;
	}
}

static struct early_suspend auto_hotplug_suspend = {
	.suspend = auto_hotplug_early_suspend,
	.resume = auto_hotplug_late_resume,
};

int __init auto_hotplug_init(void)
{
	pr_info("auto_hotplug: v0.220 by _thalamus\n");
	pr_info("auto_hotplug: rev 1 enhanced by motley\n");
	pr_info("auto_hotplug: rev 1.1 modified by pengus77\n");
	pr_info("auto_hotplug: %d CPUs detected\n", CPUS_AVAILABLE);

	INIT_DELAYED_WORK_DEFERRABLE(&hotplug_online_all_work, hotplug_online_all_work_fn);
	INIT_DELAYED_WORK_DEFERRABLE(&hotplug_offline_all_work, hotplug_offline_all_work_fn);

	register_early_suspend(&auto_hotplug_suspend);

	return 0;
}
late_initcall(auto_hotplug_init);

#endif /* CONFIG_HAS_EARLYSUSPEND */