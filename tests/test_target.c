#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

volatile int g_global_counter = 42;

void* thread_worker(void* arg) {
    pthread_setname_np(pthread_self(), "WorkerThread1");
    for (int i = 0; i < 50; ++i) {
        usleep(100000);
    }
    return NULL;
}

int calculate_fib(int n) {
    if (n <= 1) return n;
    return calculate_fib(n - 1) + calculate_fib(n - 2);
}

void print_secret_message(int iteration, const char* label) {
    int local_var = iteration * 10;
    printf("[%s] Loop %d: local_var = %d, global = %d, fib(7) = %d\n",
           label, iteration, local_var, g_global_counter, calculate_fib(7));
    g_global_counter += 1;
}

int main(int argc, char* argv[]) {
    const char* label = (argc > 1) ? argv[1] : "Worker";
    printf("[Target] Started PID %d with label '%s'\n", getpid(), label);

    pthread_t th;
    pthread_create(&th, NULL, thread_worker, NULL);

    for (int i = 1; i <= 100; ++i) {
        print_secret_message(i, label);
        usleep(800000); // sleep 0.8s
    }

    pthread_join(th, NULL);
    printf("[Target] Finished.\n");
    return 0;
}
