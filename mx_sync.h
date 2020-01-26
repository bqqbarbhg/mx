#ifndef MX_SYNC_H_INCLUDED
#define MX_SYNC_H_INCLUDED

#include <stdint.h>

// -- Global semaphore pool
// Returned indices are guaranteed to be around the same
// as the number of concurrently allocated semaphores.

uint32_t mx_sema_pool_alloc();
void mx_sema_pool_free(uint32_t ix);
void mx_sema_pool_wait(uint32_t ix);
void mx_sema_pool_signal(uint32_t ix);
void mx_sema_pool_wait_n(uint32_t ix, uint32_t num);
void mx_sema_pool_signal_n(uint32_t ix, uint32_t num);
void mx_sema_pool_clean();

// -- Automatically allocating/freeing pooled semaphore

typedef struct mx_pooled_sema mx_pooled_sema;
struct mx_pooled_sema {
	uint32_t state;
};

void mx_pooled_sema_wait(mx_pooled_sema *ps);
void mx_pooled_sema_signal(mx_pooled_sema *ps);
void mx_pooled_sema_wait_n(mx_pooled_sema *ps, uint32_t num);
void mx_pooled_sema_signal_n(mx_pooled_sema *ps, uint32_t num);

// -- Mutex

typedef struct mx_mutex mx_mutex;
struct mx_mutex {
	uint32_t state;
};

void mx_mutex_lock(mx_mutex *m);
int mx_mutex_try_lock(mx_mutex *m);
void mx_mutex_lock_spin(mx_mutex *m, uint32_t spin);
void mx_mutex_unlock(mx_mutex *m);
int mx_mutex_is_locked(mx_mutex *m);

void mx_mutex_set_default_spin(uint32_t spin);

// -- Recursive mutex

typedef struct mx_recursive_mutex mx_recursive_mutex;
struct mx_recursive_mutex {
	mx_mutex mutex;
	uint32_t thread_id;
	uint32_t recursion_depth;
};

uint32_t mx_recursive_mutex_lock(mx_recursive_mutex *m);
uint32_t mx_recursive_mutex_try_lock(mx_recursive_mutex *m);
uint32_t mx_recursive_mutex_lock_spin(mx_recursive_mutex *m, uint32_t spin);
uint32_t mx_recursive_mutex_unlock(mx_recursive_mutex *m);
uint32_t mx_recursive_mutex_get_depth(mx_recursive_mutex *m);

// -- Semaphore

typedef struct mx_semaphore mx_semaphore;
struct mx_semaphore {
	int32_t count;
	mx_pooled_sema sema;
};

void mx_semaphore_wait(mx_semaphore *s);
void mx_semaphore_signal(mx_semaphore *s);
void mx_semaphore_wait_n(mx_semaphore *s, uint32_t num);
void mx_semaphore_signal_n(mx_semaphore *s, uint32_t num);
int32_t mx_semaphore_get_count(mx_semaphore *s);

#endif
