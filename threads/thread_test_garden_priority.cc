/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_garden_priority.hh"
#include "semaphore.hh"
#include "lock.hh"
#include "system.hh"

#include <stdio.h>

// Semaphore* semaphore2 = new Semaphore("test2",1);
Lock* lock = new Lock("test2");

static const unsigned NUM_TURNSTILES = 10;
static const unsigned ITERATIONS_PER_TURNSTILE = 5;
static bool done[NUM_TURNSTILES];
static int count;

static void
Turnstile(void *n_)
{
    unsigned *n = (unsigned *) n_;

    for (unsigned i = 0; i < ITERATIONS_PER_TURNSTILE; i++) {
        lock->Acquire();
        int temp = count;
        printf("Turnstile %u yielding with temp=%u.\n", *n, temp);
        currentThread->Yield();
        printf("Turnstile %u back with temp=%u.\n", *n, temp);
        count = temp + 1;
        lock->Release();
        currentThread->Yield();
    }
    printf("Turnstile %u finished. Count is now %u.\n", *n, count);
    done[*n] = true;
}

void
ThreadTestGardenPriority()
{
    //Launch a new thread for each turnstile 
    //(except one that will be run by the main thread)

    char **names = new char*[NUM_TURNSTILES];
    unsigned *values = new unsigned[NUM_TURNSTILES];
    for (unsigned i = 0; i < NUM_TURNSTILES; i++) {
        printf("Launching turnstile %u.\n", i);
        names[i] = new char[16];
        sprintf(names[i], "Turnstile %u", i);
        printf("Name: %s\n", names[i]);
        values[i] = i;
        Thread *t = new Thread(names[i],1,i);
        t->Fork(Turnstile, (void *) &(values[i]));
    }
   
    // Wait until all turnstile threads finish their work.  `Thread::Join` is
    // not implemented at the beginning, therefore an ad-hoc workaround is
    // applied here.
    for (unsigned i = 0; i < NUM_TURNSTILES; i++) {
        while (!done[i]) {
            currentThread->Yield();
        }
    }

    printf("All turnstiles finished. Final count is %u (should be %u).\n",
           count, ITERATIONS_PER_TURNSTILE * NUM_TURNSTILES);

    // Free all the memory
    for (unsigned i = 0; i < NUM_TURNSTILES; i++) {
	delete[] names[i];
    }
    delete []values;
    delete []names;
}
