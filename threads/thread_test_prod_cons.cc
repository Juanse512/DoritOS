/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "thread_test_prod_cons.hh"
#include "lock.hh"
#include "condition.hh"
#include "system.hh"
#include <stdio.h>
#include <unistd.h>

int buffer[BUFFER_SIZE];
int counter = 0;
int finished[2];

Lock *bufferLock;
Condition *notFull;
Condition *notEmpty;

static void producer(void *np)
{
    for (int i = 0; i < MAX_ELEMENTS; i++)
    {
        bufferLock->Acquire();
        
        while (counter == BUFFER_SIZE) notFull->Wait();
        
        printf("Produciendo %d\n", i);
        
        buffer[counter] = i;
        counter++;
        
        notEmpty->Signal();
        bufferLock->Release();
    }
    finished[0] = 1;
}
static void consumer(void *np)
{
    for (int i = 0; i < MAX_ELEMENTS; i++)
    {
        bufferLock->Acquire();
        
        while (counter == 0) notEmpty->Wait();
        
        printf("Consumiendo %d\n", buffer[counter - 1]);
        
        counter--;
        
        notFull->Signal();
        bufferLock->Release();
    }
    finished[1] = 1;
}

void ThreadTestProdCons()
{
    finished[0] = 0;
    finished[1] = 0;
    
    printf("Iniciando test de productor-consumidor\n");
    
    bufferLock = new Lock("bufferLock");
    notFull = new Condition("notFull", bufferLock);
    notEmpty = new Condition("notEmpty", bufferLock);
    
    Thread *cons = new Thread("consumer");
    Thread *prod = new Thread("producer");

    cons->Fork(consumer, NULL);
    prod->Fork(producer, NULL);
    
    while (!finished[0] || !finished[1]) currentThread->Yield();
}
