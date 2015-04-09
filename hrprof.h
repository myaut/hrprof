/*
 * hrprof.h
 *
 *  Created on: Feb 22, 2014
 *      Author: myaut
 */

#ifndef HRPROF_H_
#define HRPROF_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/perf_event.h>
#include <linux/hrtimer.h>

// #define HRPROF_DEMO				1
#define HRPROF_QUEUE_LEN		40

typedef struct hrprof_cpu {
	struct perf_event* event;
	spinlock_t lock;

	struct hrtimer hrtimer;

	int head;
	int tail;

	u64 queue[HRPROF_QUEUE_LEN];
} hrprof_cpu_t;

#define HRPROF_MODE_LLC				1
#define HRPROF_MODE_TLB				2
#define HRPROF_MODE_NODE			3
#define HRPROF_MODE_INSTRUCTIONS	4

#define HRPROF_MODE_MASK			0xff

#define HRPROF_MODE_WRITE_FLAG		0x1000

#endif /* HRPROF_H_ */
