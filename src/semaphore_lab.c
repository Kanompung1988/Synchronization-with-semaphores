#include "semaphore_lab.h"

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TOKENS_PER_THREAD 10

static sem_t output_lock;

/* Returned by worker() when a semaphore call fails (any non-NULL value). */
static int worker_failed;

/* sem_wait() can be interrupted by a signal (EINTR); retry in that case. */
static int lock_output(void) {
    while (sem_wait(&output_lock) == -1) {
        if (errno != EINTR) {
            perror("sem_wait");
            return -1;
        }
    }
    return 0;
}

static int unlock_output(void) {
    if (sem_post(&output_lock) == -1) {
        perror("sem_post");
        return -1;
    }
    return 0;
}

void *worker(void *arg) {
    char token = *(char *)arg;
    int index;

    for (index = 0; index < TOKENS_PER_THREAD; index++) {
        /* Enter the critical section: only one thread may print at a time. */
        if (lock_output() == -1) {
            return &worker_failed;
        }

        /* Bracketed print section: must never interleave with the other thread. */
        printf("[%c", token);
        fflush(stdout);
        usleep(1000);
        printf("]");
        fflush(stdout);

        /* Leave the critical section so the other thread can print. */
        if (unlock_output() == -1) {
            return &worker_failed;
        }
    }

    return NULL;
}

int main(void) {
    pthread_t first_thread;
    pthread_t second_thread;
    char first_token = 'A';
    char second_token = 'B';
    void *first_result = NULL;
    void *second_result = NULL;

    /* pshared = 0 (shared between threads of this process), initial value = 1 (unlocked). */
    if (sem_init(&output_lock, 0, 1) == -1) {
        perror("sem_init");
        return 1;
    }

    if (pthread_create(&first_thread, NULL, worker, &first_token) != 0) {
        perror("pthread_create");
        sem_destroy(&output_lock);
        return 1;
    }
    if (pthread_create(&second_thread, NULL, worker, &second_token) != 0) {
        perror("pthread_create");
        /* The first thread is already running: wait for it before tearing down. */
        pthread_join(first_thread, NULL);
        sem_destroy(&output_lock);
        return 1;
    }

    if (pthread_join(first_thread, &first_result) != 0) {
        perror("pthread_join");
        return 1;
    }
    if (pthread_join(second_thread, &second_result) != 0) {
        perror("pthread_join");
        return 1;
    }

    if (sem_destroy(&output_lock) == -1) {
        perror("sem_destroy");
        return 1;
    }

    if (first_result != NULL || second_result != NULL) {
        fprintf(stderr, "worker thread failed\n");
        return 1;
    }

    printf("\ndone\n");
    return 0;
}
