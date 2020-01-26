#include "../mx_sync.h"

#include <thread>
#include <vector>
#include <inttypes.h>

mx_mutex mutex;
uint64_t num;

mx_semaphore sema;

#define NUM 100000

void thread()
{
	for (uint64_t i = 0; i < NUM; i++) {
		mx_mutex_lock(&mutex);
		num++;
		mx_mutex_unlock(&mutex);
	}

	mx_semaphore_signal(&sema);
}

int main(int argc, char **argv)
{
	std::vector<std::thread> threads;
	threads.resize(10);
	for (std::thread &t : threads) {
		t = std::thread(thread);
	}

	while (!mx_semaphore_try_wait_n(&sema, (uint32_t)threads.size())) {
		printf("%" PRIu64 "\n", num);
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}

	printf("%" PRIu64 "\n", num);
	return 0;
}
