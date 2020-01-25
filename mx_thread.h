#pragma once

#include <stdint.h>

void mx_wait_semaphore(uintptr_t id);
void mx_signal_semaphore(uintptr_t id);
void mx_update_semaphore(uintptr_t id, int32_t delta);

typedef struct mx_mutex mx_mutex;
struct mx_mutex {
	uint32_t state;
};

void mx_mutex_lock(mx_mutex *m);

typedef struct mx_semaphore mx_semaphore;
struct mx_semaphore {
	int32_t count;
};

void mx_semaphore_wait(mx_semaphore *s, uint32_t num);
int mx_semaphore_try_wait(mx_semaphore *s, uint32_t num);
void mx_semaphore_signal(mx_semaphore *s, uint32_t num);

#if 0

typedef struct mx_rw_mutex mx_rw_mutex;
struct mx_rw_mutex {
	uint32_t read_lock;
	uint32_t write_lock;
	int32_t read_count;
};

void mx_rw_mutex_lock_read(mx_rw_mutex *m);
void mx_rw_mutex_unlock_read(mx_rw_mutex *m);
void mx_rw_mutex_lock_write(mx_rw_mutex *m);
void mx_rw_mutex_unlock_write(mx_rw_mutex *m);

typedef struct mx_semaphore mx_semaphore;
struct mx_semaphore {
	uint64_t state;
};

void mx_semaphore_wait(mx_semaphore *s, uint32_t num);
int mx_semaphore_try_wait(mx_semaphore *s, uint32_t num);
void mx_semaphore_signal(mx_semaphore *s, uint32_t num);
int32_t mx_semaphore_get_count(const mx_semaphore *s);

#endif
