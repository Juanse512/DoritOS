/// Condition variables, a synchronization primitive
///
/// A data structure for synchronizing threads.
///
/// Base Nachos only provides the interface for this primitive.  In order to
/// use it, an implementation has to be written.
///
/// All synchronization objects have a `name` parameter in the constructor;
/// its only aim is to ease debugging the program.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2000      José Miguel Santos Espino - ULPGC.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH


#include "lock.hh"
#include "semaphore.hh"

class Channel {
public:

    /// Constructor: indicate which lock the condition variable belongs to.
    Channel(const char *debugName);
    ~Channel();

    void Send(int message);
    void Receive(int* message);

    const char* GetName() const;

private:
    Semaphore* sending;
    Semaphore* receiving;

    Semaphore* received;
    int buffer;

    const char* name;
};


#endif
