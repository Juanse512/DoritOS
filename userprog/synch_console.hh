#ifndef NACHOS_USERPROG_SYNCHCONSOLE__HH
#define NACHOS_USERPROG_SYNCHCONSOLE__HH

#include "machine/console.hh"
#include "threads/semaphore.hh"
#include "threads/lock.hh"
class SynchConsole {
public:
    SynchConsole();
    ~SynchConsole();
    void ReadBuffer(char *buffer, int size);
    void WriteBuffer(char *buffer, int size);
    void ReadAvailable();
    void WriteDone();
private:
    char ReadChar();        
    void WriteChar(char ch); 
    Console *console;
    Lock *readLock;
    Lock *writeLock;
    Semaphore *readAvailable;
    Semaphore *writeDone;
};


#endif