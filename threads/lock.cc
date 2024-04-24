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
    prioritiesLock = new Semaphore("prioritiesLock", 1);
    owner = nullptr;
    oldPriority = 0;
    highestPriority = 0;
    for (int i = 0; i < 10; i++) {
        priorities[i] = 0;
    }
}

Lock::~Lock()
{
    delete name;
    delete lock;
    delete prioritiesLock;
}

const char *
Lock::GetName() const
{
    return name;
}

// Apartado 5b

// No tiene sentido hablar de problema de inversion sobre semaforos
// ya que estos no poseen un dueño como los locks, cualquier hilo
// puede liberar un recurso a traves de semaforos, por lo que no es 
// aparente a cual hilo se le debe asignar la prioridad alta.

void
Lock::Acquire()
{   
    ASSERT(!IsHeldByCurrentThread());

    prioritiesLock->P();
    priorities[currentThread->GetPriority()]++;
    UpdateHighestPriority();
    if (owner && (highestPriority > owner->GetPriority())) owner->SetPriority(highestPriority);
    prioritiesLock->V();

    lock->P();
    
    owner = currentThread;
    oldPriority = owner->GetPriority();
    owner->SetPriority(highestPriority);
}

void
Lock::Release()
{
    ASSERT(IsHeldByCurrentThread());
    
    owner->SetPriority(oldPriority);

    prioritiesLock->P();
    priorities[owner->GetPriority()]--;
    UpdateHighestPriority();
    prioritiesLock->V();

    owner = nullptr;
    lock->V();
}

bool
Lock::IsHeldByCurrentThread() const
{   
    return owner == currentThread;
}

void
Lock::UpdateHighestPriority()
{
    highestPriority = 0;
    for (int i = 9; i >= 0; i--) {
        if (priorities[i] > 0) {
            highestPriority = i;
            break;
        }
    }

    DEBUG('t', "%s highest priority: %d\n", GetName(), highestPriority);
}
