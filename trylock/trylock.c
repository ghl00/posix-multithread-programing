#include <pthread.h>
#include "../errors.h"

#define SPIN 1000000000

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
long counter;
time_t end_time;

/*
 * Thread start routine that repeatedly locks a mutex and
 * increments a counter.
 */

void *counter_thread(void *arg)
{
	int status;
	int spin;

	/*
	 * Until end_time, increment the counter each second. Instead of
	 * just incrementing the counter, it sleeps for another second
	 * with the mutex locked, to give monitor_thread a reasonable
	 * chance of running.
	 */
	 while (time(NULL) < end_time)
	 {
	 	status = pthread_mutex_lock(&mutex);
	 	if(status != 0)
	 		err_abort(status, "Lock mutex");
	 	for (spin=0; spin<SPIN; spin++)
	 		counter++;
	 	status = pthread_mutex_unlock(&mutex);
	 	if(status != 0)
	 		err_abort(status, "Unlock mutex");
	 	sleep(1);
	 }
	 printf("Counter is %#1x\n", counter);
}

/*
 * Thread start routine to "monitor" the counter. Every 3
 * seconds, try to lock the mutex and read the counter.If the
 * trylock fails, skip this cycle.
 */
 void *monitor_thread(void *arg)
 {
 	int status;
 	int misses = 0;

 	/*
 	 * Lopp until end_time, checking the counter every 3 seconds.
 	 */
	while(time(NULL) < end_time)
	{
		sleep(3);
		status = pthread_mutex_trylock(&mutex);
		if(status != EBUSY)
		{
			if (status != 0)
				err_abort(status, "Trylock mutex");
			printf("Counter is %ld\n",counter/SPIN);
			status = pthread_mutex_unlock(&mutex);
			if (status != 0)
				err_abort(status, "Unlock mutex");
		}
		else
			misses++;		/*Count "misees" on the lock */
	}
	printf("Monitor thread missed update %d times.\n", misses);
	return NULL; 
 }

int main(int argc, char *argv[])
{
	int status;
	pthread_t counter_thread_id;
	pthread_t monitor_thread_id;

	end_time = time(NULL) + 60;		/* Run for 1 minute */
	status = pthread_create(&counter_thread_id, NULL, counter_thread, NULL);
	if (status != 0)
		err_abort(status, "Create counter thread");

	status = pthread_create(&monitor_thread_id, NULL, monitor_thread, NULL);
	if (status != 0)
		err_abort(status, "Create monitor thread");

	status = pthread_join(counter_thread_id, NULL);
	if (status != 0)
		err_abort(status, "Join counter thread");

	status = pthread_join(monitor_thread_id, NULL);
	if (status != 0)
		err_abort(status, "Join monitor thread");
 	return 0;
}
