/**********************************************************************
 * Copyright (c) 2020
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

/* THIS FILE IS ALL YOURS; DO WHATEVER YOU WANT TO DO IN THIS FILE */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "list_head.h"

/**
 * The process which is currently running
 */
#include "process.h"
extern struct process *current;

/**
 * List head to hold the processes ready to run
 */
extern struct list_head readyqueue;


/**
 * Resources in the system.
 */
#include "resource.h"
extern struct resource resources[NR_RESOURCES];


/**
 * Monotonically increasing ticks
 */
extern unsigned int ticks;


/**
 * Quiet mode. True if the program was started with -q option
 */
extern bool quiet;


/***********************************************************************
 * Default FCFS resource acquision function
 *
 * DESCRIPTION
 *   This is the default resource acquision function which is called back
 *   whenever the current process is to acquire resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
bool fcfs_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

/***********************************************************************
 * Default FCFS resource release function
 *
 * DESCRIPTION
 *   This is the default resource release function which is called back
 *   whenever the current process is to release resource @resource_id.
 *   The current implementation serves the resource in the requesting order
 *   without considering the priority. See the comments in sched.h
 ***********************************************************************/
void fcfs_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter  is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}



#include "sched.h"

/***********************************************************************
 * FIFO scheduler
 ***********************************************************************/
static int fifo_initialize(void)
{
	return 0;
}

static void fifo_finalize(void)
{
}

static struct process *fifo_schedule(void)
{
	struct process *next = NULL;

	/* You may inspect the situation by calling dump_status() at any time */
	// dump_status();

	/*
	 * When there was no process to run in the previous tick (so does
	 * in the very beginning of the simulation), there will be
	 * no @current process. In this case, pick the next without examining
	 * the current process. Also, when the current process is blocked
	 * while acquiring a resource, @current is (supposed to be) attached
	 * to the waitqueue of the corresponding resource. In this case just
	 * pick the next as well.
	 */
	if (!current || current->status == PROCESS_WAIT) {
		goto pick_next;
	}

	/* The current process has remaining lifetime. Schedule it again */
	if (current->age < current->lifespan) {
		return current;
	}

pick_next:
	/* Let's pick a new process to run next */

	if (!list_empty(&readyqueue)) {
		/**
		 * If the ready queue is not empty, pick the first process
		 * in the ready queue
		 */
		next = list_first_entry(&readyqueue, struct process, list);
		/**
		 * Detach the process from the ready queue. Note we use list_del_init()
		 * instead of list_del() to maintain the list head tidy. Otherwise,
		 * the framework will complain (assert) on process exit.
		 */
		list_del_init(&next->list);
	}

	/* Return the next process to run */
	return next;
}

struct scheduler fifo_scheduler = {
	.name = "FIFO",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
	.initialize = fifo_initialize,
	.finalize = fifo_finalize,
	.schedule = fifo_schedule,
};


/***********************************************************************
 * SJF scheduler
 ***********************************************************************/
static struct process *sjf_schedule(void)
{
	struct process *sjfProc = list_first_entry(&readyqueue, struct process, list);
	struct process *sjfSmallest = NULL;
	unsigned int lsChk = sjfProc->lifespan;
	if(current)
		{
			if(current->age < current->lifespan) //current가 끝날때까지 더돌아야함
		 		return current;
		}

	if(!list_empty(&readyqueue)) //readyqueue 존재
		{
			list_for_each_entry(sjfProc, &readyqueue, list) //돌면서 제일작은애찾고 반환
			{
				if(sjfProc->lifespan < lsChk)
				{
                	lsChk = sjfProc->lifespan;
					sjfSmallest = sjfProc;
				}
			}
			list_del_init(&sjfSmallest->list);
			return sjfSmallest;
		}
	else return NULL;
}

struct scheduler sjf_scheduler = {
	.name = "Shortest-Job First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = sjf_schedule, /* TODO: Assign sjf_schedule()
								to this function pointer to activate
								SJF in the system */
};

/***********************************************************************
 * SRTF scheduler
 ***********************************************************************/
static struct process *srtf_schedule(void)
{
	struct process *srtfProc = list_first_entry(&readyqueue, struct process, list);
	struct process *srtfSmallest = NULL;
	
	if(current)
		{	
			if(!list_empty(&readyqueue)) // current가 있고, 현재 readyqueue에 process존재
			{
				srtfSmallest = srtfProc;

				if(current->age < current->lifespan)
					list_add(&current->list,&readyqueue); //current가 아직 다 안 돌았으면, 맨 앞에 추가(같은 경우 돌던 애가 먼저 돌아야 함)

				list_for_each_entry(srtfProc, &readyqueue, list) //readyqueue를 돌면서, 가장 작은 lifespan인 애를 smallest로 업데이트
				{
					if(srtfProc->lifespan < srtfSmallest->lifespan)
					{
            	    	srtfSmallest = srtfProc;
					}
				}
				if(srtfSmallest->age <= srtfSmallest->lifespan) //current가 가장 lifespan이 작은데, 아직 다 돌지 못했으면 계속 돌려야 함
		 		{
					list_del_init(&srtfSmallest->list);
					return srtfSmallest;
				}
				else return NULL;
			}
			else if(current->age < current->lifespan) //readyqueu에 아무것도 없음(마지막 프로세스)가 돌고있는 상태, 끝내줘야함
		 		return current;

			else return NULL;
		}

	else if(!list_empty(&readyqueue))
	{
		srtfSmallest = srtfProc;
        list_for_each_entry(srtfProc, &readyqueue, list)//맨 처음 current를 생성
			{
				if(srtfProc->lifespan < srtfSmallest->lifespan)
				{
					srtfSmallest = srtfProc;
				}
			}
			list_del_init(&srtfSmallest->list);
			return srtfSmallest;
	}
	else return NULL;
}


struct scheduler srtf_scheduler = {
	.name = "Shortest Remaining Time First",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = srtf_schedule,
	/* You need to check the newly created processes to implement SRTF.
	 * Use @forked() callback to mark newly created processes */
	/* Obviously, you should implement srtf_schedule() and attach it here */
};


/***********************************************************************
 * Round-robin scheduler
 ***********************************************************************/
static struct process *rr_schedule(void)
{
	struct process *rrProc = list_first_entry(&readyqueue, struct process, list);
	struct process *nrrProc = NULL;

	if(!list_empty(&readyqueue)) 
	{
		if(current)
		{
			if(current->age < current->lifespan) //한번 돌았는데 안 끝났으면 readyqueue에 다시 추가, readyqueue 1번 애를 실행시킴
			{
				nrrProc = current;
				list_add_tail(&nrrProc->list,&readyqueue);
				list_del_init(&rrProc->list);
				return rrProc;
			}
			else //다 돌았으면 바로 다음애 반환
			{
				list_del_init(&rrProc->list);
				return rrProc;
			}
		}		
		else 
		{
			list_del_init(&rrProc->list);
			return rrProc;
		}
	}
	else if(current->age < current->lifespan) //맨처음 current 생성
		{
			return current;
		}
	else return NULL;
}

struct scheduler rr_scheduler = {
	.name = "Round-Robin",
	.acquire = fcfs_acquire, /* Use the default FCFS acquire() */
	.release = fcfs_release, /* Use the default FCFS release() */
	.schedule = rr_schedule,
	/* Obviously, you should implement rr_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler
 ***********************************************************************/
static struct process *prio_schedule(void)
{
    struct process *prioProc = list_first_entry(&readyqueue, struct process, list);
	struct process *highest = NULL;

	if(current) //current 존재
    	{
			if(!list_empty(&readyqueue)) //readyqueue에 다음 프로세스 존재
			{
				highest = prioProc; //highest는 일단 처음거
				
				if(current->age < current->lifespan) //아직 current가 돌 필요가 있으면
					list_add_tail(&current->list,&readyqueue); //list에 맨 뒤에 다시 추가(같은 경우 얘는 나중에 다시 돌아야 함)
				
				list_for_each_entry(prioProc, &readyqueue, list) //readyqueue를 보면서 prio비교한 후 제일 높은 애를 highest로 갱신
				{
					if(prioProc->prio > highest->prio) //prio같은경우 여기 안들어가짐, 번갈아가면서 돔
					{
						highest = prioProc;
					}
				}
				if(highest->age <= highest->lifespan) //highest가 더 돌아야 한다면 highest 반환
					{
						list_del_init(&highest->list);
						return highest;
					}
				else return NULL;
			}
			else if(current->age < current->lifespan) //내가 마지막인데 더돌아야됨
					return current;
				
				else return NULL;
		}
	else if(!list_empty(&readyqueue))
	    {
			highest = prioProc;
			list_for_each_entry(prioProc, &readyqueue, list)
		    {
		        if(prioProc->prio > highest->prio)
				{
					highest = prioProc;
				}
		    }
			list_del_init(&highest->list);
			return highest;
		}
	else return NULL;
}

struct scheduler prio_scheduler = {
	.name = "Priority",
	.acquire = fcfs_acquire,
	.release = fcfs_release,
    .schedule = prio_schedule,
	/**
	 * Implement your own acqure/release function to make priority
	 * scheduler correct.
	 */
	/* Implement your own prio_schedule() and attach it here */
};


/***********************************************************************
 * Priority scheduler with priority ceiling protocol
 ***********************************************************************/
bool pcp_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;

	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		current->prio = MAX_PRIO;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

void prio_release(int resource_id)
{
	struct resource *r = resources + resource_id;

	/* Ensure that the owner process is releasing the resource */
	assert(r->owner == current);

	r->owner->prio = r->owner->prio_orig;

	/* Un-own this resource */
	r->owner = NULL;

	/* Let's wake up ONE waiter (if exists) that came first */
	if (!list_empty(&r->waitqueue)) {
		struct process *waiter =
				list_first_entry(&r->waitqueue, struct process, list);

		/**
		 * Ensure the waiter  is in the wait status
		 */
		assert(waiter->status == PROCESS_WAIT);

		/**
		 * Take out the waiter from the waiting queue. Note we use
		 * list_del_init() over list_del() to maintain the list head tidy
		 * (otherwise, the framework will complain on the list head
		 * when the process exits).
		 */
		list_del_init(&waiter->list);

		/* Update the process status */
		waiter->status = PROCESS_READY;

		/**
		 * Put the waiter process into ready queue. The framework will
		 * do the rest.
		 */
		list_add_tail(&waiter->list, &readyqueue);
	}
}

struct scheduler pcp_scheduler = {
	.name = "Priority + Priority Ceiling Protocol",
	.acquire = pcp_acquire,
	.release = prio_release,
    .schedule = prio_schedule,
	/**
	 * Implement your own acqure/release function too to make priority
	 * scheduler correct.
	 */
};


/***********************************************************************
 * Priority scheduler with priority inheritance protocol
 ***********************************************************************/
bool pip_acquire(int resource_id)
{
	struct resource *r = resources + resource_id;
	
	if (!r->owner) {
		/* This resource is not owned by any one. Take it! */
		r->owner = current;
		return true;
	}

	/* OK, this resource is taken by @r->owner. */

	/* Update the current process state */
	current->status = PROCESS_WAIT;

	/* And append current to waitqueue */
	list_add_tail(&current->list, &r->waitqueue);

	/**
	 * And return false to indicate the resource is not available.
	 * The scheduler framework will soon call schedule() function to
	 * schedule out current and to pick the next process to run.
	 */
	return false;
}

struct scheduler pip_scheduler = {
	.name = "Priority + Priority Inheritance Protocol",
	.acquire = pip_acquire,
	.release = prio_release,
    .schedule = prio_schedule,
	/**
	 * Ditto
	 */
};
