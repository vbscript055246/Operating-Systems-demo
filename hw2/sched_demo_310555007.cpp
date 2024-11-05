#include <iostream>
#include <string>
#include <sstream>
#include <getopt.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>

#define DEBUG_parser 0
#define CPUID 1

using namespace std;

typedef struct {
    pthread_t thread_id;
    int thread_num;
    int sched_policy;
    int sched_priority;
    double wait_sec;
} thread_info_t;

void CPU_setting_init();
void main_thread_sched();
int do_work(int threads_num, string policies, string priorities, double wait_time);
void *thread_func(void *info);
void time_counter(double settime);
double time_clock();

void main_parser(int argc, char *argv[], int &thn, double &wt, string &prior, string &pol);
void policies_parser(int threads_num, thread_info_t *arr, string policies);
void priorities_parser(int threads_num, thread_info_t *arr, string priorities);
int usage(const char *s);

pthread_barrier_t barrier;

int main(int argc, char *argv[]) {
    int threads_num = -1;
    double wait_time = -1;
    string priorities, policies;

    main_parser(argc, argv, threads_num, wait_time, priorities, policies);
#if DEBUG_parser
    cout << "threads_num: " << threads_num << endl;
    cout << "priorities: " << priorities << endl;
    cout << "policies: " << policies << endl;
    cout << "wait_time: " << wait_time << endl;
#endif
    do_work(threads_num, policies, priorities, wait_time);
}

int do_work(int threads_num, string policies, string priorities, double wait_time){
    thread_info_t *threads_info = new thread_info_t[threads_num];
    if(threads_info == nullptr){
        perror("can't new array");
        exit(1);
    }
    // set all thread policies
    policies_parser(threads_num, threads_info, policies);
    // set all thread priorities
    priorities_parser(threads_num, threads_info, priorities);
    // set CPU
    CPU_setting_init();
    // set main thread to prior=50, SCHED_FIFO
    main_thread_sched();
    // get barrier
    if ( pthread_barrier_init(&barrier, NULL, threads_num + 1) ){
        perror("pthread_barrier_init");
        exit(1);
    }

    for (int i = 0; i < threads_num; ++i ){
        threads_info[i].thread_num = i;
        threads_info[i].wait_sec = wait_time;

        pthread_attr_t attr;
        if ( pthread_attr_init(&attr) ){
            perror("pthread_attr_init");
            exit(1);
        }

        if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)){
            perror("pthread_attr_setinheritsched");
            exit(1);
        }

        if ( pthread_attr_setschedpolicy(&attr, threads_info[i].sched_policy) ){
            perror("pthread_attr_setschedpolicy");
            exit(1);
        }
        sched_param param;
        param.sched_priority = (threads_info[i].sched_policy) ? threads_info[i].sched_priority : 0;
        if (pthread_attr_setschedparam(&attr, &param)){
            perror("pthread_attr_setschedparam");
            exit(1);
        }

        int val = pthread_create(&(threads_info[i].thread_id), &attr, thread_func, (void *) (threads_info + i));
        if(val!=0){
            cout << "pthread_create, val: " << val << endl;
            exit(1);
        }
        if(pthread_attr_destroy(&attr)){
            perror("pthread_attr_destroy");
            exit(1);
        }
    }
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < threads_num; ++i )
        pthread_join(threads_info[i].thread_id, 0);
    pthread_barrier_destroy(&barrier);
    delete[] threads_info;
    return 0;
}

void main_thread_sched(){
    sched_param param;
    param.sched_priority = 99;
    if ( sched_setscheduler(0, SCHED_FIFO, &param) == -1 ) {
        perror("sched_setscheduler");
        exit(1);
    }
}

void CPU_setting_init(){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(CPUID, &cpuset);
    if ( sched_setaffinity(getpid(), sizeof(cpuset), &cpuset) == -1 ){
        perror("sched_setaffinity");
        exit(1);
    }
}

void *thread_func(void *info) {
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < 3; ++i ){
        cout << "Thread " << ((thread_info_t *)info)->thread_num << " is running" << endl; ;
        time_counter(((thread_info_t *)info)->wait_sec);
    }
    pthread_exit(0);
}

void time_counter(double settime){
    double st = time_clock(), now;
    do{ now = time_clock(); }
    while(settime > now - st);
}

double time_clock(){
    timespec tp;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
    return tp.tv_sec + tp.tv_nsec * 0.000000001;
}

void main_parser(int argc, char *argv[], int &thn, double &wt, string &prior, string &pol){
    for (int i = getopt(argc, argv, "n:t:s:p:h"); i != -1;i = getopt(argc, argv, "n:t:s:p:h")){
        switch ( i ){
            case 'h':
                usage(*argv);
                exit(0);
            case 'n':
                thn = stoi(string(optarg));
                break;
            case 'p':
                prior = string(optarg);
                break;
            case 's':
                pol = string(optarg);
                break;
            case 't':
                wt = stod(optarg);
                break;
            default:
                cerr << "Missing arg for " << (char)optopt << endl;
                usage(*argv);
                exit(1);
        }
    }
    if ( thn < 0 || wt < 0 || pol.empty() || prior.empty() ){
        usage(*argv);
        exit(1);
    }
}

void priorities_parser(int threads_num, thread_info_t *arr, string priorities) {
    int i=0;
    string priority;
    istringstream iss;
    iss.str(priorities);
    while(getline(iss, priority, ',')){
        arr[i].sched_priority = stoi(priority);
        i++;
    }
    if ( i != threads_num ){
        cout << "The number of priorities did not match the number of thread\n";
        exit(1);
    }
}

void policies_parser(int threads_num, thread_info_t *arr, string policies) {
    int i=0;
    string pol;
    istringstream iss;
    iss.str(policies);

    while(getline(iss, pol, ',')){
        if(pol == "FIFO")
            arr[i].sched_policy = SCHED_FIFO;
        else if(pol == "NORMAL" || pol == "OTHER")
            arr[i].sched_policy = SCHED_OTHER;
        else{
            cerr << "Policy \"" << pol << "\" is not one of the supported policies\n";
            exit(1);
        }
        i++;
    }
    if ( i != threads_num ){
        cout << "The number of policies did not match the number of thread\n";
        exit(1);
    }
}

int usage(const char *s){
    fprintf(stderr, "Usage:\n\t%s -n <num_thread> -t <time_wait> -s <polices> -p <priorities>\n", s);
    printf(
            "Options:\n"
            "\t-n <num_threads>  Number of threads to run simultaneously\n"
            "\t-t <time_wait>    Duration of \"busy\" period\n"
            "\t-s <policies>     Scheduling policy for each thread,\n"
            "\t                    currently only NORMAL(SCHED_NORMAL) and FIFO(SCHED_FIFO)\n"
            "\t                    scheduling policies are supported.\n"
            "\t-p <priorities>   Real-time thread priority for real-time threads\n"
    );
    return fprintf(stderr, "Example:\n\t%s -n 4 -t 0.5 -s NORMAL,FIFO,NORMAL,FIFO -p -1,10,-1,30\n", s);
}