/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "userprog/address_space.hh"
#include "filesys/open_file.hh"
#include "userprog/args.hh"
#include <stdio.h>
static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

void initializeThread(void * argAddr)
{
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();
    
    int argc = WriteArgs((char**) argAddr);
    int argv = machine->ReadRegister(STACK_REG); 

    machine->WriteRegister(4, argc);
    machine->WriteRegister(5, argv);

    // Needed space for preserving callee-saved registers in MIPS
    machine->WriteRegister(STACK_REG, argv - 24);

    machine->Run();
}


/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid)
    {

    case SC_EXIT:
    {
        int status = machine->ReadRegister(4);
        DEBUG('e', "Shutdown, initiated by user program with status %d.\n", status);
        currentThread->Finish();
        break;
    }

    case SC_HALT:
        DEBUG('e', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
        break;

    case SC_CREATE:
    {
        int filenameAddr = machine->ReadRegister(4);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
            machine->WriteRegister(2, -1);
            break;
        }

        char filename[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr, filename, sizeof filename))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
            machine->WriteRegister(2, -1);
            break;
        }

        DEBUG('e', "`Create` requested for file `%s`.\n", filename);
        if (!fileSystem->Create(filename, 0))
        {
            DEBUG('e', "Error: could not create file `%s`.\n", filename);
            machine->WriteRegister(2, -1);
            break;
        };

        DEBUG('e', "Created file `%s`.\n", filename);
        machine->WriteRegister(2, 0);
        break;
    }

    case SC_CLOSE:
    {
        int fid = machine->ReadRegister(4);
        DEBUG('e', "`Close` requested for id %u.\n", fid);
        if (fid < 2)
        {
            DEBUG('e', "Error: cannot close file with id %d.\n", fid);
            machine->WriteRegister(2, -1);
            break;
        }
        if (!currentThread->RemoveFile(fid))
        {
            DEBUG('e', "Error: file with id %d not found.\n", fid);
            machine->WriteRegister(2, -1);
            break;
        }
        DEBUG('e', "Closed file with id %d.\n", fid);
        machine->WriteRegister(2, 0);
        break;
    }

    case SC_OPEN:
    {
        int filenameAddr = machine->ReadRegister(4);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
            machine->WriteRegister(2, -1);
            break;
        }

        char filename[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr, filename, sizeof filename))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
            machine->WriteRegister(2, -1);
            break;
        }

        DEBUG('e', "`Open` requested for file `%s`.\n", filename);
        OpenFile *file = fileSystem->Open(filename);
        if (file == nullptr)
        {
            DEBUG('e', "Error: could not open file `%s`.\n", filename);
            machine->WriteRegister(2, -1);
            break;
        }

        OpenFileId id = currentThread->AddFile(file);
        DEBUG('e', "Opened file `%s` with id %u.\n", filename, id + 2);
        machine->WriteRegister(2, id + 2);
        break;
    }

    case SC_READ:
    {
        int buffAddr = machine->ReadRegister(4);
        int size = machine->ReadRegister(5);
        OpenFileId id = machine->ReadRegister(6);
        int bytesRead = 0;

        DEBUG('e',"`Read` requested for file with id `%d`.\n", id);
        if (id < 0)
        {
            DEBUG('e', "Error: invalid file id %d.\n", id);
            machine->WriteRegister(2, -1);
            break;
        }
        if (size < 0)
        {
            DEBUG('e', "Error: invalid size %d.\n", size);
            machine->WriteRegister(2, -1);
            break;
        }
        char bufferSys[size + 1];
        if (id == 0)
        {
            synchConsole->ReadBuffer(bufferSys, size);
            bufferSys[size] = '\0';
            bytesRead = size;
            if (bytesRead != 0)
                WriteBufferToUser(bufferSys, buffAddr, bytesRead);
            DEBUG('e', "Read from stdin: %s, %d bytes\n", bufferSys, size);
            machine->WriteRegister(2, size);
            break;
        }
        if (id == 1)
        {
            DEBUG('e', "Error: tried to read from stdout\n");
            machine->WriteRegister(2, -1);
            break;
        }
        OpenFile *file = currentThread->GetFile(id - 2);
        if (file == nullptr)
        {
            DEBUG('e', "Error: file with id %d not found.\n", id);
            machine->WriteRegister(2, -1);
            break;
        }

        bytesRead = file->Read(bufferSys, size);
        DEBUG('e', "Read %d bytes from file with id `%d`.\n", bytesRead, id);
        bufferSys[bytesRead] = '\0';
        if (bytesRead != 0)
            WriteBufferToUser(bufferSys, buffAddr, bytesRead);
        
        DEBUG('e',"Read file with id `%d`.\n", id);
        machine->WriteRegister(2, bytesRead);

        break;
    }

    case SC_WRITE:
    {
        int buffAddr = machine->ReadRegister(4);
        int size = machine->ReadRegister(5);
        OpenFileId id = machine->ReadRegister(6);

        DEBUG('e', "`Write` requested for file with id %d.\n", id);

        if (id < 0)
        {
            DEBUG('e', "Error: invalid file id %d.\n", id);
            machine->WriteRegister(2, -1);
            break;
        }

        if (size <= 0)
        {
            DEBUG('e', "Error: invalid size %d.\n", size);
            machine->WriteRegister(2, -1);
            break;
        }
    
        if (id == 0)
        {
            DEBUG('e', "Error: tried to write in stdin\n");
            machine->WriteRegister(2, -1);
            break;
        }
        char buffSys[size + 1];
        ReadBufferFromUser(buffAddr, buffSys, size);
        if (id == 1)
        {
            DEBUG('e', "Wrote to stdout: %s, %d bytes\n", buffSys, size);
            buffSys[size] = '\0';
            synchConsole->WriteBuffer(buffSys, size);
            machine->WriteRegister(2, size);
            break;
        }

        OpenFile *file = currentThread->GetFile(id - 2);
        if (file == nullptr)
        {
            DEBUG('e', "Error: file with id %d not found.\n", id);
            machine->WriteRegister(2, -1);
            break;
        }
        
        int bytesWritten = file->Write(buffSys, size);
        DEBUG('e', "Wrote file with id %d.\n", id);
        machine->WriteRegister(2, bytesWritten);
        break;
    }

    case SC_REMOVE:
    {
        int filenameAddr = machine->ReadRegister(4);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
            machine->WriteRegister(2, -1);
            break;
        }

        char filename[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr, filename, sizeof filename))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
            machine->WriteRegister(2, -1);
            break;
        }

        DEBUG('e', "`Remove` requested for file `%s`.\n", filename);

        if (fileSystem->Remove(filename)){
            DEBUG('e', "Removed file `%s`.\n", filename);
            machine->WriteRegister(2, 0);
        } else{
            DEBUG('e', "Error: could not remove file `%s`.\n", filename);
            machine->WriteRegister(2, -1);
        }
        break;
    }
    case SC_JOIN:
    {
        int tid = machine->ReadRegister(4);
        if (tid < 0)
        {
            DEBUG('e', "Error: invalid thread id %d.\n", tid);
            machine->WriteRegister(2, -1);
            break;
        }
        if (tid == currentThread->GetId())
        {
            DEBUG('e', "Error: thread %d tried to join itself.\n", tid);
            machine->WriteRegister(2, -1);
            break;
        }
        Thread *thread = threadTable->Get(tid);
        if (thread == nullptr)
        {
            DEBUG('e', "Error: thread with id %d not found.\n", tid);
            machine->WriteRegister(2, -1);
            break;
        }
        thread->Join();
        machine->WriteRegister(2, 0);
        break;
    }
    case SC_EXEC:
    {
        int filenameAddr = machine->ReadRegister(4);
        int argAddr = machine->ReadRegister(5);
        char** addrPointer = SaveArgs(argAddr);
        if (filenameAddr == 0)
        {
            DEBUG('e', "Error: address to filename string is null.\n");
            machine->WriteRegister(2, -1);
            break;
        }

        char filename[FILE_NAME_MAX_LEN + 1];
        if (!ReadStringFromUser(filenameAddr, filename, sizeof filename))
        {
            DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n", FILE_NAME_MAX_LEN);
            machine->WriteRegister(2, -1);
            break;
        }

        DEBUG('e', "`Exec` requested for file `%s`.\n", filename);
        OpenFile *executable = fileSystem->Open(filename);
        if (executable == nullptr)
        {
            DEBUG('e', "Error: could not open file `%s`.\n", filename);
            machine->WriteRegister(2, -1);
            break;
        }

        if(addrPointer == nullptr)
        {
            DEBUG('e', "Error: error reading arguments.\n");
            machine->WriteRegister(2, -1);
            break;
        }

        Thread *thread = new Thread(filename, 1);
        
        AddressSpace *space = new AddressSpace(executable, thread->GetId());
        
        ASSERT(thread != nullptr);
        ASSERT(space != nullptr);

        thread->space = space;
        
        thread->Fork(initializeThread, (void*) (addrPointer));
        
        machine->WriteRegister(2, thread->GetId());
        break;
    }
    default:
        fprintf(stderr, "Unexpected system call: id %d.\n", scid);
        ASSERT(false);
    }

    IncrementPC();
}

unsigned int tlb_index = 0;
static void PageFaultHandler(ExceptionType et)
{
    unsigned vaddr = machine->ReadRegister(BAD_VADDR_REG);
    DEBUG('e', "Page fault at vaddress %d.\n", vaddr);
    int addr = vaddr / PAGE_SIZE;
    DEBUG('e', "Page fault at address %u.\n", vaddr);
    // podriamos llegar a necesitar el puntero de el address space 
    #ifdef DEMAND_LOADING
        DEBUG('e', "Page %d, valid:%d.\n", addr, currentThread->space->GetPageTable(addr).valid);
        if(!currentThread->space->GetPageTable(addr).valid){
            DEBUG('e', "Loading Page %u.\n", addr);
            stats->numPageFaults++;
            currentThread->space->LoadPage(addr);
        }
    #endif
    machine->GetMMU()->tlb[tlb_index] = currentThread->space->GetPageTable(addr);
    
    tlb_index = (tlb_index+1) % TLB_SIZE; 

    stats->tlbMiss++;
}

/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION, &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION, &SyscallHandler);
#ifdef USE_TLB
    machine->SetHandler(PAGE_FAULT_EXCEPTION, &PageFaultHandler);
#else
    machine->SetHandler(PAGE_FAULT_EXCEPTION, &DefaultHandler);
#endif
    machine->SetHandler(READ_ONLY_EXCEPTION, &DefaultHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION, &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
