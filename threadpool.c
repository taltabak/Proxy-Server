#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


threadpool* create_threadpool(int num_threads_in_pool)
{
    if(num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool < 1) //case of invalid argument.
    {
        printf("invalid num threads in pool");
        return NULL;       
    }
    threadpool* tp = (threadpool*)malloc(sizeof(threadpool));
    if(tp == NULL)
    {
        perror("MALLOC FAILED");
        return NULL;
    }
    
    //INIT THREADPOOL:
    tp->num_threads = num_threads_in_pool;
    tp->qsize = 0;
    tp->shutdown = 0;
    tp->dont_accept = 0;
    tp->qhead = NULL;
    tp->qtail = NULL;
    tp->threads = (pthread_t*)malloc(sizeof(pthread_t)*(num_threads_in_pool));
    if(tp->threads == NULL)
    {
        perror("MALLOC FAILED");
        return NULL;
    }
    pthread_mutex_init(&(tp->qlock), NULL);
    pthread_cond_init(&(tp->q_not_empty), NULL); 
    pthread_cond_init(&(tp->q_empty), NULL); 
    //CREATING THE THREADS:
    for(int i = 0; i < tp->num_threads ; i++)
        pthread_create((tp->threads)+i,NULL,do_work,tp);
    return tp;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
    if(from_me == NULL) //case of invalid argument
        return;
    pthread_mutex_lock(&(from_me->qlock)); //take over control of the threadpool
    if(from_me->dont_accept)    //case destroying the threadpool has already begun.
    {
        pthread_mutex_unlock(&(from_me->qlock));
        return;
    }
    //INIT AND ENQUEUE WORK STRUCTURE:
    work_t* work = (work_t*)malloc(sizeof(work_t));
    if(work == NULL)
    {
        perror("MALLOC FAILED");
        return;
    }
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;
    if(from_me->qsize == 0) //case its the first element in queue
        from_me->qhead = work;
    else    
        from_me->qtail->next = work;
    from_me->qtail = work;
    (from_me->qsize)++;
    
    pthread_cond_signal(&(from_me->q_not_empty));   //signal to one of the threads (start work function) 
    pthread_mutex_unlock(&(from_me->qlock));
}

void* do_work(void* p)
{
    if(p == NULL)   //case of invalid argument
    {
        printf("invalid argument");
        return NULL;
    }
    threadpool* tp = (threadpool*)p;
    while(1)
    {
        pthread_mutex_lock(&(tp->qlock));   //take over control of the threadpool
        if(tp->shutdown)   //case destroying the threadpool has already begun.
        {
            pthread_mutex_unlock(&(tp->qlock));
            return NULL;
        }
        if(tp->qsize == 0)  //case queue is empty
        {
            pthread_cond_wait(&(tp->q_not_empty),&(tp->qlock)); 
        }
        if(tp->shutdown)    //case destroying the threadpool has already begun. (check after waiting)
        {
            pthread_mutex_unlock(&(tp->qlock));
            return NULL;
        }
        if((tp->qsize) > 0) //prevent decreasing qsize to -1 and segmentaion fault. 
        {
            work_t* work = tp->qhead;
            (tp->qsize)--;
            if(work == NULL)
                return NULL;
            tp->qhead = tp->qhead->next;
            if(work->routine == NULL)
            {
                free(work);
                return NULL;
            }
            pthread_mutex_unlock(&(tp->qlock));
            work->routine(work->arg);
            pthread_mutex_lock(&(tp->qlock));
            free(work);    
        }
        if(tp->qsize == 0)  //alert queue is empty
            pthread_cond_signal(&(tp->q_empty)); 
        pthread_mutex_unlock(&(tp->qlock));
    }
}

void destroy_threadpool(threadpool* destroyme)
{
    if(destroyme == NULL)   //case of invalid argument
    {
        printf("invalid argument");
        return;
    }
    pthread_mutex_lock(&(destroyme->qlock));    //take over control of the threadpool
    destroyme->dont_accept = 1; //alert dispatch function to not accept anymore work
    if((destroyme->qsize) > 0)  //case there are still work in the queue.
    {
        pthread_cond_wait(&(destroyme->q_empty),&(destroyme->qlock));   //wait for the work to be done.
    }
    destroyme->shutdown = 1;    //alert that destroying the threadpool is beginning and
    pthread_cond_broadcast(&(destroyme->q_not_empty));  //signal all waiting threads (the threads check shutdown flag) 
    pthread_mutex_unlock(&(destroyme->qlock));  
    //TERMINATING THE THREADS:
    void* retval;   //pointer to hold exit status of the threads.
    for(int i = 0; i < destroyme->num_threads ; i++)
    {
        pthread_join((destroyme->threads)[i],&retval);
    }
    //DEALLOCATING THREADPOOL:
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    pthread_cond_destroy(&(destroyme->q_empty));
    free(destroyme->threads);
    free(destroyme);
}