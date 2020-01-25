#include "../mx_os.h"
#include "../mx_thread.h"

#include <stdio.h>
#include <stdint.h>
#include <Windows.h>

#define NTHREADS 10
#define NUM 100000

mx_semaphore sema;
mx_mutex mutex;
uint32_t num;

void thread(void *user)
{
	for (uint32_t i = 0; i < NUM; i++) {
		mx_mutex_lock(&mutex);
		num++;
		mx_mutex_unlock(&mutex);
	}
	mx_semaphore_signal(&sema, 1);
}

int main(int argc, char **argv)
{
	mx_os_thread *threads[NTHREADS];
	for (uint32_t i = 0; i < NTHREADS; i++) {
		threads[i] = mx_os_thread_start(thread, NULL);
	}

	while (!mx_semaphore_try_wait(&sema, NTHREADS)) {
		printf(".. %u\n", num);
		Sleep(250);
	}

	for (uint32_t i = 0; i < NTHREADS; i++) {
		mx_os_thread_join(threads[i]);
	}

	printf(".. %u\n", num);

	return 0;
}
