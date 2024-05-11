
#include "userprog/synch_console.hh"

static void
read(void *arg){
    ASSERT(arg != NULL);
    SynchConsole *synchConsole = (SynchConsole *)arg;
    synchConsole->ReadAvailable();
}

static void
write(void *arg){
    ASSERT(arg != NULL);
    SynchConsole *synchConsole = (SynchConsole *)arg;
    synchConsole->WriteDone();
}

SynchConsole::SynchConsole()
{
    readAvailable = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    readLock = new Lock("read lock");
    writeLock = new Lock("write lock");
    console = new Console(nullptr, nullptr, read, write, this);
}

SynchConsole::~SynchConsole()
{
    delete console;
    delete writeLock;
    delete readLock;
    delete writeDone;
    delete readAvailable;
}

void SynchConsole::WriteChar(char ch)
{
    console->PutChar(ch);
    writeDone->P();
}

char SynchConsole::ReadChar()
{
    readAvailable->P();
    char ch = console->GetChar();
    return ch;
}

void SynchConsole::ReadBuffer(char *buffer, int size)
{
    ASSERT(buffer != NULL);
    ASSERT(size > 0);
    readLock->Acquire();
    for (int i = 0; i < size; i++)
    {
        buffer[i] = ReadChar();
        if (buffer[i] == '\n')
        {
            break;
        }
    }
    readLock->Release();
    return;
}

void SynchConsole::WriteBuffer(char *buffer, int size)
{
    ASSERT(buffer != NULL);
    ASSERT(size > 0);
    writeLock->Acquire();
    for (int i = 0; i < size; i++)
    {
        WriteChar(buffer[i]);
    }
    writeLock->Release();
}

void SynchConsole::ReadAvailable(){
    readAvailable->V();
}

void SynchConsole::WriteDone(){
    writeDone->V();
}