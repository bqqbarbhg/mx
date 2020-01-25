#include "mx_os.h"
#include "mx_intrin_win32.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

// -- mx_os_thread

struct mx_os_thread {
	HANDLE handle;
	mx_os_thread_func func;
	void *user;
};

static DWORD WINAPI mx_win32_thread_entry(LPVOID arg)
{
	mx_os_thread *thread = (mx_os_thread*)arg;
	thread->func(thread->user);
	return 0;
}

mx_os_thread *mx_os_thread_start(mx_os_thread_func func, void *user)
{
	mx_os_thread *thread = (mx_os_thread*)malloc(sizeof(mx_os_thread));
	if (!thread) return NULL;
	thread->func = func;
	thread->user = user;
	thread->handle = CreateThread(NULL, 0, &mx_win32_thread_entry, thread, 0, NULL);
	return thread;
}

void mx_os_thread_join(mx_os_thread *thread)
{
	WaitForSingleObject(thread->handle, INFINITE);
	CloseHandle(thread->handle);
	free(thread);
}

// -- mx_os_semaphore

void mx_os_semaphore_init(mx_os_semaphore *s)
{
	s->state = (uintptr_t)CreateSemaphoreA(NULL, 0, 1, NULL);
}

void mx_os_semaphore_wait(mx_os_semaphore *s)
{
	WaitForSingleObject((HANDLE)s->state, INFINITE);
}

void mx_os_semaphore_signal(mx_os_semaphore *s)
{
	ReleaseSemaphore((HANDLE)s->state, 1, NULL);
}

void mx_os_semaphore_update(mx_os_semaphore *s, int32_t delta)
{
	mx_assert(delta != 0);
	if (delta > 0) {
		ReleaseSemaphore((HANDLE)s->state, delta, NULL);
	} else {
		do {
			WaitForSingleObject((HANDLE)s->state, INFINITE);
		} while (++delta != 0);
	}
}

void mx_os_semaphore_free(mx_os_semaphore *s)
{
	CloseHandle((HANDLE)s->state);
	s->state = 0;
}
