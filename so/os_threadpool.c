// SPDX-License-Identifier: BSD-3-Clause
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "os_threadpool.h"
#include "os_list.h"



/* === TASK === */

pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_mutex_t cond_mutex;
int totalThreads;

/* Creates a task that thread must execute */
os_task_t *task_create(void *arg, void (*f)(void *))
{

	os_task_t *to_add = (os_task_t *)malloc(sizeof(os_task_t));

	to_add->argument = arg;
	to_add->task = f;
	return to_add;
}

/* Add a new task to threadpool task queue */
void add_task_in_queue(os_threadpool_t *tp, os_task_t *t)
{
	os_task_queue_t *rear = tp->tasks;

	if (rear == NULL) {
		pthread_mutex_lock(&(tp->taskLock));
		os_task_queue_t *rear = malloc(sizeof(os_task_queue_t));

		/* I used just the os_task_queue_t from the threadpool to keep all the tasks */
		rear->task = t;
		rear->next = NULL;

		tp->tasks = rear;
		/* announce that a task has been added to queue */
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&(tp->taskLock));
		return;
	}

	pthread_mutex_lock(&(tp->taskLock));
	while (rear->next != NULL)
		rear = rear->next;

	os_task_queue_t *new_node = malloc(sizeof(os_task_queue_t));

	new_node->task = t;
	new_node->next = NULL;
	rear->next = new_node;
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&(tp->taskLock));
}

/* Get the head of task queue from threadpool */
os_task_t *get_task(os_threadpool_t *tp)
{
	os_task_queue_t *rear = malloc(sizeof(os_task_queue_t));

	rear = tp->tasks;

	/* retrun NULL if there are no tasks in the queue */
	if (tp->tasks == NULL)
		return NULL;

	if (rear->next != NULL) {
		os_task_queue_t *pop = malloc(sizeof(os_task_queue_t));

		pop = tp->tasks;
		tp->tasks = tp->tasks->next;
		return pop->task;
	}

	os_task_queue_t *pop = malloc(sizeof(os_task_queue_t));

	pop = tp->tasks;
	tp->tasks = NULL;
	return pop->task;
}

/* === THREAD POOL === */

/* Initialize the new threadpool */
os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads)
{
	os_threadpool_t *pool = (os_threadpool_t *)malloc(sizeof(os_threadpool_t));

	pthread_mutex_init(&(pool->taskLock), NULL);
	pthread_mutex_init(&(cond_mutex), NULL);
	pthread_cond_init(&cond, NULL);

	totalThreads = nThreads;

	/* using num_thread to count all the finished jobs */
	pool->num_threads = 0;

	pool->should_stop = 0;
	pool->tasks = NULL;
	pool->threads = malloc(sizeof(pthread_t) * nThreads);

	for (unsigned int i = 0; i < totalThreads; i++)
		pthread_create(&(pool->threads[i]), NULL, thread_loop_function, (void *)pool);

	return pool;
}

/* Loop function for threads */
void *thread_loop_function(void *args)
{

	while (1) {
		os_threadpool_t *pool = (os_threadpool_t *)args;

		/* wait for a task */
		pthread_mutex_lock(&(cond_mutex));
		while (pool->tasks == NULL && pool->should_stop == 0)
			pthread_cond_wait(&cond, &cond_mutex);

		pthread_mutex_unlock(&(cond_mutex));

		if (pool->should_stop == 1 && pool->tasks == NULL) {
			pthread_exit(NULL);
			break;
		}

		pthread_mutex_lock(&(mutex));

		os_task_t *to_execute = malloc(sizeof(os_task_t));

		to_execute = get_task(pool);

		/* mark as done */
		pool->num_threads = pool->num_threads + 1;

		/* execute the task */
		if (to_execute != NULL)
			to_execute->task(to_execute->argument);

		pthread_mutex_unlock(&(mutex));
	}
}

/* Stop the thread pool once a condition is met */
void threadpool_stop(os_threadpool_t *tp, int (*processingIsDone)(os_threadpool_t *))
{
	/* Waiting for any potential remaining task to fisih */
	while (processingIsDone(tp) != 1)
		;

	pthread_mutex_lock(&(tp->taskLock));
	tp->should_stop = 1;
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&(tp->taskLock));

	/* Wait for all threads to exit */ 
	for (unsigned int i = 0; i < totalThreads; i++)
		pthread_join(tp->threads[i], NULL);
}
