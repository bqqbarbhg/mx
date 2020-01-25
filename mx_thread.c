#include "mx_thread.h"
#include "mx_intrin_win32.h"
#include "mx_os.h"

#include <stdlib.h>
#include <string.h>

#define SEMA_BLOCK_SIZE 4
#define SEMA_NUM_ROOTS 7

typedef struct {
	mx_os_semaphore os_sema;
	uintptr_t pad;
	uintptr_t id_counts[2];
} sema;

typedef struct sema_block {
	sema sema[SEMA_BLOCK_SIZE];
	struct sema_block *next;
} sema_block;

typedef struct {
	sema_block block;
	mx_os_semaphore os_sema;
	uint32_t lock;
} sema_root;

sema_root *g_sema_roots[SEMA_NUM_ROOTS];

int try_lock_sema_root(sema_root *root, int spin)
{
	if (mxa_cas32(&root->lock, 0, 1)) return 1;
	while (spin-- > 0) {
		uint32_t state = mxa_load32(&root->lock);
		if (state > 1) break;
		if (mxa_cas32(&root->lock, 0, 1)) return 1;
		mx_yield();
	}
	return 0;
}

void lock_sema_root(sema_root *root)
{
	uint32_t count = mxa_inc32(&root->lock);
	if (count > 0) {
		mx_os_semaphore_wait(&root->os_sema);
	}
}

void unlock_sema_root(sema_root *root)
{
	uint32_t count = mxa_dec32(&root->lock);
	if (count > 1) {
		mx_os_semaphore_signal(&root->os_sema);
	}
}

int lock_sema(uintptr_t prev_id, uintptr_t id, sema *s)
{
	uintptr_t cas_cmp[2], cas_val[2];
	cas_cmp[0] = prev_id;
	cas_val[0] = id;

	// Increment sema refcount and add `delta`
	do {
		uintptr_t s_id = mxa_load_uptr(&s->id_counts[0]);
		if (s_id != prev_id) return 0;
		uintptr_t counts = mxa_load_uptr(&s->id_counts[1]);
		cas_cmp[1] = counts;
		cas_val[1] = counts + 1;
	} while (!mxa_double_cas(&s->id_counts, cas_cmp, cas_val));

	return 1;
}

void update_sema(uintptr_t id, sema *s, int32_t delta)
{
	uintptr_t cas_cmp[2], cas_val[2];
	cas_cmp[0] = id;

	// Update the OS semaphore
	mx_os_semaphore_update(&s->os_sema, delta);

	// Decrement the refcount and remove `delta`
	// if the refcount and count are zero mark the sema as free.
	do {
		uintptr_t s_id = mxa_load_uptr(&s->id_counts[0]);
		mx_assert(s_id == id);
		uintptr_t counts = mxa_load_uptr(&s->id_counts[1]);
		uintptr_t new_counts = counts - 1 + (uintptr_t)((intptr_t)delta << 16);
		cas_cmp[1] = counts;
		cas_val[0] = new_counts != 0 ? id : 0;
		cas_val[1] = new_counts;
	} while (!mxa_double_cas(&s->id_counts, cas_cmp, cas_val));
}

sema *find_sema(sema_root *root, uintptr_t id)
{
	sema_block *block = &root->block;
	while (block) {
		for (uint32_t i = 0; i < SEMA_BLOCK_SIZE; i++) {
			sema *s = &block->sema[i];
			if (mxa_load_uptr(&s->id_counts[0]) == id) {
				if (lock_sema(id, id, s)) {
					return s;
				}
			}
		}
		block = (sema_block*)mxa_load_ptr(&block->next);
	}
	return NULL;
}

sema *create_sema(sema_root *root, uintptr_t id)
{
	sema_block *block = &root->block;
	for (;;) {
		for (uint32_t i = 0; i < SEMA_BLOCK_SIZE; i++) {
			sema *s = &block->sema[i];
			if (mxa_load_uptr(&s->id_counts[0]) == 0) {
				if (lock_sema(0, id, s)) {
					return s;
				}
			}
		}

		sema_block *next = (sema_block*)mxa_load_ptr(&block->next);
		if (!block) {
			next = (sema_block*)_aligned_malloc(sizeof(sema_block) + 63, 64);
			mx_assert(next != NULL);
			memset(next, 0, sizeof(sema_block));
			for (uint32_t i = 0; i < SEMA_BLOCK_SIZE; i++) {
				mx_os_semaphore_init(&next->sema[i].os_sema);
			}
			int res = mxa_cas_ptr(&block->next, NULL, next);
			mx_assert(res != 0);
		}
		block = next;
	}
}

sema *resolve_sema(sema_root *root, uintptr_t id)
{
	// 1. Try to find the semaphore
	sema *sem = find_sema(root, id);
	if (sem) return sem;

	// 2. Try to lock and find/create the semaphore
	if (try_lock_sema_root(root, 500)) {
		sem = find_sema(root, id);
		if (!sem) sem = create_sema(root, id);
		unlock_sema_root(root);
		if (sem) return sem;
	}

	// 3. Try to find the semaphore without lock again
	sem = find_sema(root, id);
	if (sem) return sem;

	// 4. Finally wait for the lock and find/create the semaphore
	lock_sema_root(root);
	sem = find_sema(root, id);
	if (!sem) sem = create_sema(root, id);
	unlock_sema_root(root);
	return sem;
}

void update_sema_root(uintptr_t id, int32_t delta)
{
	sema_root **p_root = &g_sema_roots[id % SEMA_NUM_ROOTS];
	sema_root *root = (sema_root*)mxa_load_ptr(p_root);
	if (root == NULL) {
		root = (sema_root*)_aligned_malloc(sizeof(sema_root) + 63, 64);
		memset(root, 0, sizeof(sema_root));
		mx_os_semaphore_init(&root->os_sema);
		for (uint32_t i = 0; i < SEMA_BLOCK_SIZE; i++) {
			mx_os_semaphore_init(&root->block.sema[i].os_sema);
		}
		if (!mxa_cas_ptr(p_root, NULL, root)) {
			mx_os_semaphore_free(&root->os_sema);
			for (uint32_t i = 0; i < SEMA_BLOCK_SIZE; i++) {
				mx_os_semaphore_free(&root->block.sema[i].os_sema);
			}
			_aligned_free(root);
			root = mxa_load_ptr(p_root);
		}
	}

	sema *sem = resolve_sema(root, id);
	update_sema(id, sem, delta);
}

void mx_wait_semaphore(uintptr_t id)
{
	mx_assert(id != 0);
	update_sema_root(id, -1);
}

void mx_signal_semaphore(uintptr_t id)
{
	mx_assert(id != 0);
	update_sema_root(id, +1);
}

void mx_update_semaphore(uintptr_t id, int32_t delta)
{
	mx_assert(id != 0);
	mx_assert(delta != 0);
	update_sema_root(id, delta);
}

// -- mx_mutex

void mx_mutex_lock(mx_mutex *m)
{
	if (mxa_inc32(&m->state) != 0) {
		mx_wait_semaphore((uintptr_t)m);
	}
}

void mx_mutex_unlock(mx_mutex *m)
{
	if (mxa_dec32(&m->state) > 1) {
		mx_signal_semaphore((uintptr_t)m);
	}
}

// -- mx_semaphore

void mx_semaphore_wait(mx_semaphore *s, uint32_t num)
{
	int32_t count = (int32_t)mxa_sub32((uint32_t*)&s->count, num) - (int32_t)num;
	if (count < 0) {
		int32_t wait = -count <= (int32_t)num ? -count : (int32_t)num; 
		mx_update_semaphore((uintptr_t)s, -wait);
	}
}

int mx_semaphore_try_wait(mx_semaphore *s, uint32_t num)
{
	int32_t count, snum = (int32_t)num;
	do {
		count = (int32_t)mxa_load32((uint32_t*)&s->count);
		if (count < snum) return 0;
	} while (!mxa_cas32((uint32_t*)&s->count, count, count - snum));
	return 1;
}

void mx_semaphore_signal(mx_semaphore *s, uint32_t num)
{
	int32_t prev = (int32_t)mxa_add32((uint32_t*)&s->count, num);
	if (prev < 0) {
		int32_t release = -prev <= (int32_t)num ? -prev : (int32_t)num;
		mx_update_semaphore((uintptr_t)s, release);
	}
}

#if 0
typedef struct {
	mx_os_semaphore sema;
	uintptr_t id_count[2];
} sema;

#define SEMA_BLOCK_SIZE 4
#define SEMA_NUM_ROOTS 7

typedef struct {
	sema sema[SEMA_BLOCK_SIZE];
	sema_block *next;
} sema_block;

typedef struct {
	sema_block block;
	uint32_t revision;
} sema_root;

sema_root g_sema_root[SEMA_NUM_ROOTS];

sema *find_sema(uintptr_t id)
{
	mx_assert(id != 0 && id != UINTPTR_MAX);

	sema_root *root = &g_sema_root[id % SEMA_NUM_ROOTS];
	uintptr_t cas_cmp[2], cas_val[2];

	for (;;) {
		uint32_t rev = mxa_load32(&root->revision);

		// Try to find the semaphore
		sema_block *block = &root->block;
		while (block) {
			for (uint32_t i = 0; i < SEMA_BLOCK_SIZE; i++) {
				sema *s = &block->sema[i];
				do {
					uintptr_t s_id = mxa_load_uptr(&s->id_count[0]);
					if (s_id != id) break;
					uintptr_t count = mxa_load_uptr(&s->id_count[1]);
					cas_cmp[0] = id;
					cas_cmp[1] = count;
					cas_val[0] = id;
					cas_val[1] = count + 1;
				} while (!mxa_double_cas(&s->id_count, cas_cmp, cas_val));
			}

			block = (sema_block*)mxa_load_ptr(&block->next);
		}

		// No point in trying to create it if revision has increased
		if (rev != mxa_load32(&root->revision)) continue;

		block = &root->block;
		while (block) {
			for (uint32_t i = 0; i < SEMA_BLOCK_SIZE; i++) {
				sema *s = &block->sema[i];
				uintptr_t s_id = mxa_load_uptr(&s->id_count[0]);
				if (s_id != 0) continue;

				// Try to initialize the semaphore
				if (s->sema.state == NULL) {
					if (mxa_cas_uptr(&s->id_count[0], 0, UINTPTR_MAX)) {
						mx_os_semaphore_init(&s->sema);
						// CAS to 0 must succeed since UINTPTR_MAX -> ? transition
						// is impossible except here
						int res = mxa_cas64(&s->id_count[0], UINTPTR_MAX, 0);
						mx_assert(res != 0);
					}
				}

				cas_cmp[0] = 0;
				cas_val[0] = 0;
				cas_cmp[0] = 0;
				cas_val[0] = 0;

			}

			// Create a new block if necessary
			sema_block *next = (sema_block*)mxa_load_ptr(&block->next);
			if (next == NULL) {
				next = (sema_block*)malloc(sizeof(sema_block));
				mx_assert(next != NULL);
				memset(next, 0, sizeof(sema_block));
				if (!mxa_cas_ptr(&block->next, NULL, next)) {
					free(next);
					next = (sema_block*)mxa_load_ptr(&block->next);
				}
			}
			block = next;
		}

	}
}

void mx_wait_semaphore(uintptr_t id)
{
}

void mx_release_semaphore(uintptr_t id)
{
}

#endif

#if 0

// TODO: mx_waitq_signal_semaphore_final() should free the semaphore after all the threads have left

uint32_t mx_waitq_alloc();
void mx_waitq_join_semaphore(uint32_t wq, uint32_t num);
void mx_waitq_signal_semaphore(uint32_t wq, uint32_t num);
void mx_waitq_signal_semaphore_final(uint32_t wq, uint32_t num);
void mx_waitq_free(uint32_t wq);

void mx_rw_mutex_lock_read(mx_rw_mutex *m)
{
	int32_t num = (int32_t)mxa_inc32(&m->read_count);
}

void mx_rw_mutex_unlock_read(mx_rw_mutex *m)
{
}

void mx_rw_mutex_lock_write(mx_rw_mutex *m)
{
	for (;;) {
	}
}

void mx_rw_mutex_unlock_write(mx_rw_mutex *m)
{
}

void mx_semaphore_wait(mx_semaphore *s, uint32_t num)
{
	if (num == 0) return;
	for (;;) {
		uint64_t state = mxa_load64(&s->state);
		uint32_t waitq = (uint32_t)(state >> 32u);
		uint32_t count = (uint32_t)state;

		if (waitq == 0) {
			if (count >= num) {
				// Semaphore is open and has enough count for us
				if (mxa_cas64(&s->state, state, state - num)) {
					// OK: Open -> Open (count - num)
					return;
				}
			} else {
				// Semaphore is open but we don't fit, allocate a
				// waitqueue and wait with the remaining count.
				uint32_t num_left = num - count;
				waitq = mx_waitq_alloc();
				uint64_t new_state = (uint64_t)waitq << 32u | num_left;
				if (mxa_cas64(&s->state, state, new_state)) {
					// OK: Open -> Locked (num - count)
					mx_waitq_join_semaphore(waitq, num_left);
					return;
				} else {
					mx_waitq_free(waitq);
				}
			}
		} else {
			// Join the waiting queue
			if (mxa_cas64(&s->state, state, state + num)) {
				// OK: Locked -> Locked (count + num)
				mx_waitq_join_semaphore(waitq, num);
				return;
			}
		}
	}
}

int mx_semaphore_try_wait(mx_semaphore *s, uint32_t num)
{
	if (num == 0) return;
	for (;;) {
		uint64_t state = mxa_load64(&s->state);
		uint32_t waitq = (uint32_t)(state >> 32u);
		uint32_t count = (uint32_t)state;
		if (waitq == 0 && count >= num) {
			// Semaphore is open and has enough count for us
			if (mxa_cas64(&s->state, state, state - num)) {
				// OK: Open -> Open (count - num)
				return 1;
			}
		} else {
			return 0;
		}
	}
}

void mx_semaphore_signal(mx_semaphore *s, uint32_t num)
{
	if (num == 0) return;
	for (;;) {
		uint64_t state = mxa_load64(&s->state);
		uint32_t waitq = (uint32_t)(state >> 32u);
		uint32_t count = (uint32_t)state;

		if (waitq == 0) {
			// Semaphore is open so we can just add count
			if (mxa_cas64(&s->state, state, state + num)) {
				// OK: Open -> Open (count + num)
				return;
			}
		} else {
			// Semaphore is locked so we need to signal some threads
			if (num < count) {
				// Release N threads without opening the semaphore
				if (mxa_cas64(&s->state, state, state - num)) {
					// OK: Locked -> Locked (count - num > 0)
					mx_waitq_signal_semaphore(waitq, num);
					return;
				}
			} else {
				// Release all the threads and transition to open
				uint64_t new_state = num - count;
				if (mxa_cas64(&s->state, state, new_state)) {
					// OK: Locked -> Open (num - count)
					mx_waitq_signal_semaphore_final(waitq, count);
					return;
				}
			}
		}

	}
}

int32_t mx_semaphore_get_count(const mx_semaphore *s)
{
	uint64_t state = mxa_load64(&s->state);
	uint32_t waitq = (uint32_t)(state >> 32u);
	int32_t count = (int32_t)(uint32_t)state;
	return waitq ? -count : count;
}

#endif
