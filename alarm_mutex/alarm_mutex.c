#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errors.h"

typedef struct alarm_tag
{
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;   /*seconds from EPOCH*/
    char                message[64];
}alarm_t;

pthread_mutex_t alarm_mutex =  PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;

void *alarm_thread(void *arg)
{
    alarm_t *alarm;
    int sleep_time;
    time_t now;
    int status;

    /**
     * Loop forever, processing Commands. The alarm thread will
     * be disintegrated when the process exits.
     */
    while(1) {
        status = pthread_mutex_lock(&alarm_mutex);
        if(status!=0)
            err_abort(status, "Lock mutex");
        alarm = alarm_list;

        /**
         * If the alarm list is empty, wait for one second. This
         * allows the main thread to run, and read another
         * command. If the list is not empty. remove the first
         * item. Compute the number of seconds to wait -- if the
         * result is less than 0 (the time has passed), then set 
         * the sleep_time to 0.
         */
        if (alarm == NULL)
            sleep_time = 1;
        else{
            alarm_list = alarm->link;
            now = time(NULL);
            if(alarm->time <= now)
                sleep_time = 0;
            else
                sleep_time = alarm->time - now;
#ifdef DEBUG
            printf("[waiting: %d(%d)\"%s\"]\n", alarm->time, sleep_time, alarm->message);
#endif
        }

        /**
         * Unlock the mutex before waiting, so that the main 
         * thread can lock it to insert a new alarm request. If
         * the sleep_time is 0, the call sched_yield, giving
         * the main thread a chance to run if it has been 
         * readied by user input, without delaying the message
         * if there's no input.
         */
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0)
            err_abort(status, "Unlock mutex");
        if (sleep_time > 0)
            sleep(sleep_time);
        else
            sched_yield(); 

        /**
         * If a timer expired, print the message and free the
         * structure.
         */
        if (alarm != NULL) {
            printf("(%d) %s\n", alarm->seconds, alarm->message);
            free(alarm);
        }
    }
}

int main(int argc, char *argv[])
{
    int status;
    char line[128];
    alarm_t *alarm, **last, *next;
    pthread_t thread;

    status = pthread_create(&thread, NULL ,alarm_thread, NULL);
    if (status != 0)
        err_abort(status, "Create alarm thread");
    while(1) {
        printf("alarm> ");
        if (fgets(line, sizeof(line), stdin) == NULL) exit(0);
        if (strlen(line) <= 1) continue;
        alarm = (alarm_t*)malloc(sizeof(alarm_t));
        if (alarm == NULL)
            errno_abort("Allocate alarm");
        
        if (sscanf(line, "%d %64[^\n]", &alarm->seconds, alarm->message) < 2) {
            fprintf(stderr, "Bad command\n");
            free(alarm);
        } else{
            status = pthread_mutex_lock(&alarm_mutex);
            if (status != 0)
                err_abort(status, "Lock mutex");
            alarm->time = time(NULL) + alarm->seconds;

            last = &alarm_list;
            next = *last;
            while (next != NULL) {
                if (next->time >= alarm->time) {
                    alarm->link = next;
                    *last = alarm;
                    break;
                }
                last = &next->link;
                next = next->link;
            }

            if (next == NULL) {
                *last = alarm;
                alarm->link = NULL;
            }
#ifdef DEBUG
            printf("[list: ");
            for (next = alarm_list; next != NULL; next = next->link)
                printf("%d(%d)[\"%s\"]", next->time, next->time-time(NULL), next->message);
            printf("]\n");
#endif
            status = pthread_mutex_unlock(&alarm_mutex);
            if(status != 0)
                err_abort(status, "Unlock mutex");
        }
    }
}
