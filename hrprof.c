#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/printk.h>

#include "hrprof.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sergey Klyaus <myaut@tune-it.ru>");
MODULE_DESCRIPTION("hrprof is a High-Resolution application profiler");

static int hrprof_last_cpu = -1;

/**
 * Resolution of hrtimer; by default - 200us
 */
unsigned long hrprof_resolution = 200000;
EXPORT_SYMBOL_GPL(hrprof_resolution);
module_param(hrprof_resolution, ulong, S_IRWXU);

/**
 * hrprof modes
 */
unsigned int hrprof_mode = HRPROF_MODE_LLC;
module_param(hrprof_mode, uint, S_IRWXU);

struct perf_event_attr pe_attr = {
#ifndef HRPROF_DEMO
	.type = PERF_TYPE_HW_CACHE,
	.config = PERF_COUNT_HW_CACHE_LL |
		(PERF_COUNT_HW_CACHE_OP_READ		<<  8) |
		(PERF_COUNT_HW_CACHE_RESULT_MISS	<< 16),
#else
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_CPU_CLOCK,
#endif
	{ .sample_period=1000000ULL }
};

static DEFINE_PER_CPU(hrprof_cpu_t, hrprof_cpu_event);

static void overflow_handler(struct perf_event * event,
				struct perf_sample_data * data,
				struct pt_regs *regs) {
	/* NOTHING */
}

static enum hrtimer_restart hrprof_timer_notify(struct hrtimer *hrtimer) {
	u64 val;
	u64 oruns;
	unsigned long flags;
	hrprof_cpu_t* event = this_cpu_ptr(&hrprof_cpu_event);

	/* XXX: is this call precise? */
	oruns = hrtimer_forward_now(hrtimer, ns_to_ktime(hrprof_resolution));

#ifndef HRPROF_DEMO
	rdpmcl(event->event->hw.idx, val);
#else
	val = local64_read(&event->event->count) + atomic64_read(&event->event->child_count);
#endif

	spin_lock_irqsave(&event->lock, flags);

	do {
		/* Advance queue indexes. If queue is overrun, lose oldest value */
		++event->tail;
		if(event->tail == HRPROF_QUEUE_LEN) {
			event->tail = 0;
		}
		if(event->tail == event->head) {
			++event->head;
			if(event->head == HRPROF_QUEUE_LEN) {
				event->head = 0;
			}
		}

		event->queue[event->tail] = val;
		val = 0;
	} while(--oruns != 0);

	spin_unlock_irqrestore(&event->lock, flags);

	return HRTIMER_RESTART;
}

int hrprof_get(u64* array, int n) {
	int count = 0;

	hrprof_cpu_t* event = this_cpu_ptr(&hrprof_cpu_event);
	unsigned long flags;

	int idx;
	int aidx = n - 1;

	spin_lock_irqsave(&event->lock, flags);

	idx = event->tail;

	while(aidx >= 0 && idx != event->head) {
		array[aidx] = event->queue[idx];
		++count;

		--idx;
		--aidx;

		if(idx == -1) {
			idx = HRPROF_QUEUE_LEN;
		}
	}

	event->head = 0;
	event->tail = 0;

	spin_unlock_irqrestore(&event->lock, flags);

	return count;
}
EXPORT_SYMBOL_GPL(hrprof_get);

static void hrprof_start_timer(void* info) {
	hrprof_cpu_t* event = this_cpu_ptr(&hrprof_cpu_event);

	hrtimer_start(&event->hrtimer, ns_to_ktime(hrprof_resolution),
				  HRTIMER_MODE_REL_PINNED);
}

static int hrprof_init_event(int cpu) {
	hrprof_cpu_t* event = per_cpu_ptr(&hrprof_cpu_event, cpu);

	spin_lock_init(&event->lock);

	event->head = 0;
	event->tail = 0;

	event->event = perf_event_create_kernel_counter(
			&pe_attr, cpu, NULL, overflow_handler, NULL);

	if(IS_ERR(event->event)) {
		return PTR_ERR(event->event);
	}

	hrtimer_init(&event->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	event->hrtimer.function = hrprof_timer_notify;

	return 0;
}

static void hrprof_destroy_event(int cpu) {
	hrprof_cpu_t* event = per_cpu_ptr(&hrprof_cpu_event, cpu);

	hrtimer_cancel(&event->hrtimer);
	perf_event_release_kernel(event->event);
}

int hrprof_init(void) {
	int cpu = 0;
	int err;

#ifndef HRPROF_DEMO
	if(hrprof_mode == HRPROF_MODE_INSTRUCTIONS) {
		pe_attr.type = PERF_TYPE_HARDWARE;
		pe_attr.config = PERF_COUNT_HW_STALLED_CYCLES_FRONTEND;
	}
	else {
		pe_attr.type = PERF_TYPE_HW_CACHE;
		pe_attr.config = PERF_COUNT_HW_CACHE_RESULT_MISS << 16;

		if(hrprof_mode & HRPROF_MODE_WRITE_FLAG) {
			pe_attr.config |= PERF_COUNT_HW_CACHE_OP_WRITE << 8;
		}
		else {
			pe_attr.config |= PERF_COUNT_HW_CACHE_OP_READ << 8;
		}

		switch(hrprof_mode & HRPROF_MODE_MASK) {
		case HRPROF_MODE_LLC:
			pe_attr.config |= PERF_COUNT_HW_CACHE_LL;
			break;
		case HRPROF_MODE_TLB:
			pe_attr.config |= PERF_COUNT_HW_CACHE_DTLB;
			break;
		case HRPROF_MODE_NODE:
			pe_attr.config |= PERF_COUNT_HW_CACHE_NODE;
			break;
		default:
			return -EINVAL;
		}
	}
#endif

	get_online_cpus();
	for_each_online_cpu(cpu) {
		err = hrprof_init_event(cpu);

		if(err != 0) {
			printk(KERN_WARNING "hrprof: Failed to register perf event (type %lu config %llx)\n",
					(unsigned long) pe_attr.type, (unsigned long long) pe_attr.config);

			hrprof_last_cpu = cpu;
			return err;
		}
	}
	put_online_cpus();

	on_each_cpu(hrprof_start_timer, NULL, 0);

	printk(KERN_INFO "hrprof: Registered perf event (type %lu config %llx)\n",
			(unsigned long) pe_attr.type, (unsigned long long) pe_attr.config);

	return 0;
}

void hrprof_exit(void) {
	int cpu = 0;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		if(cpu == hrprof_last_cpu)
			break;

		hrprof_destroy_event(cpu);
	}
	put_online_cpus();
}

module_init(hrprof_init);
module_exit(hrprof_exit);
