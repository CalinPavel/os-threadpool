// SPDX-License-Identifier: BSD-3-Clause
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_list.h"
#include "os_threadpool.h"

#define MAX_TASK 100000
#define MAX_THREAD 4

int sum;
os_graph_t *graph;
os_threadpool_t *pool;
pthread_mutex_t node_mutex;
int done;
int total_tasks;

void tracking_tasks(void)
{
	done++;
}

int processingIsDone(os_threadpool_t *tp)
{
	/* Check if all tasks have been completed */ 
	if (total_tasks <= 1)
		return 1;
	return 0;
}

void compute(int value)
{
	pthread_mutex_lock(&(node_mutex));
	sum += value;
	total_tasks--;
	pthread_mutex_unlock(&(node_mutex));
}

/* add tasks to queue */
void *processNode(unsigned int nodeIdx)
{
	os_node_t *node = graph->nodes[nodeIdx];

	compute(node->nodeInfo);
	for (int i = 0; i < node->cNeighbours; i++) {
		pthread_mutex_lock(&(node_mutex));
		if (graph->visited[node->neighbours[i]] == 0) {
			graph->visited[node->neighbours[i]] = 1;
			os_task_t *new_task = task_create((char *)node->neighbours[i], (void *)processNode);

			add_task_in_queue(pool, new_task);
			tracking_tasks();
		}
		pthread_mutex_unlock(&(node_mutex));
	}
}

void traverse_graph(void)
{
	for (int i = 0; i < graph->nCount; i++) {
		if (graph->visited[i] == 0) {
			graph->visited[i] = 1;
			processNode(i);
		}
	}
}

int main(int argc, char *argv[])
{

	pthread_mutex_init(&(node_mutex), NULL);

	if (argc != 2) {
		printf("Usage: input_file\n");
		exit(1);
	}

	FILE *input_file1 = fopen(argv[1], "r");

	int nCount;

	if (fscanf(input_file1, "%d", &nCount) == 0)
		printf("[ERROR] Can't read from file\n");

	FILE *input_file = fopen(argv[1], "r");

	if (input_file == NULL) {
		printf("[Error] Can't open file\n");
		return -1;
	}

	graph = create_graph_from_file(input_file);
	if (graph == NULL) {
		printf("[Error] Can't read the graph from file\n");
		return -1;
	}


	total_tasks = nCount;
	pool = threadpool_create(MAX_TASK, MAX_THREAD);
	traverse_graph();
	threadpool_stop(pool, processingIsDone);

	printf("%d", sum);
	return 0;
}
