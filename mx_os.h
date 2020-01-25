#pragma once

#include <stdint.h>

typedef void (*mx_os_thread_func)(void *user);

typedef struct mx_os_thread mx_os_thread;
mx_os_thread *mx_os_thread_start(mx_os_thread_func func, void *user);
void mx_os_thread_join(mx_os_thread *thread);

typedef struct mx_os_semaphore mx_os_semaphore;
struct mx_os_semaphore {
	uintptr_t state;
};

void mx_os_semaphore_init(mx_os_semaphore *s);
void mx_os_semaphore_wait(mx_os_semaphore *s);
void mx_os_semaphore_signal(mx_os_semaphore *s);
void mx_os_semaphore_update(mx_os_semaphore *s, int32_t delta);
void mx_os_semaphore_free(mx_os_semaphore *s);
