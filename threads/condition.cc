/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "condition.hh"
#include "system.hh"

Condition::Condition(const char *debugName, Lock *conditionLock)
{
    lock = conditionLock;
    waiting = 0; 
    condition = new Semaphore(debugName, 0);
    waitingLock = new Lock("waitingLock");
}

Condition::~Condition()
{

    delete condition;
    delete waitingLock;
    delete name;

}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{

    waitingLock->Acquire();
    waiting++;
    waitingLock->Release();

    lock->Release();
    condition->P();
    lock->Acquire();

}

void
Condition::Signal()
{
    waitingLock->Acquire();

    if(waiting > 0){
        condition->V();
        waiting--;
    }

    waitingLock->Release();
}

void
Condition::Broadcast()
{
    waitingLock->Acquire();

    for(; waiting > 0; waiting--){
        condition->V();
    }

    waitingLock->Release();
}