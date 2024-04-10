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

#include <stdio.h>
#include "lock.hh"
#include "system.hh"
#include "lib/assert.hh"

Lock::Lock(const char *debugName)
{
    name = debugName;
    lock = new Semaphore(name, 1);
    owner = nullptr;    
}

Lock::~Lock()
{
    delete name;
    delete lock;
}

const char *
Lock::GetName() const
{
    return name;
}

void
Lock::Acquire()
{
    ASSERT(!IsHeldByCurrentThread());

    lock->P();
    owner = currentThread;
}

void
Lock::Release()
{
    ASSERT(IsHeldByCurrentThread());
    
    owner = nullptr;
    lock->V();
}

bool
Lock::IsHeldByCurrentThread() const
{   
    return owner == currentThread;
}
