/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"
#include "machine/system_dep.hh"
#include <string.h>
#include <cstdio>

unsigned int 
AddressSpace::TranlateAddress(unsigned int virtualAddr){
    unsigned int page = virtualAddr / PAGE_SIZE;
    unsigned int offset = virtualAddr % PAGE_SIZE;
    uint32_t physicalPage = pageTable[page].physicalPage;
    return (physicalPage * PAGE_SIZE) + offset;
}

/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file, int _pid) : pid(_pid)
{
    ASSERT(executable_file != nullptr);
    #ifdef SWAP
    head = 0;
    #endif
    exe = new Executable(executable_file);
    ASSERT(exe->CheckMagic());

    // How big is address space?

    unsigned size = exe->GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;
    #ifndef SWAP
    ASSERT(numPages <= machine->GetNumPhysicalPages());
    #endif
      // Check we are not trying to run anything too big -- at least until we
      // have virtual memory.

    DEBUG('a', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.

    DEBUG('e', "Allocating %u pages for new address space.\n", numPages);
    
    #ifdef SWAP
      swapMap = new Bitmap(numPages);
    #endif

    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        #ifdef USER_PROGRAM
          #ifndef DEMAND_LOADING
            int physicalPage = pages->Find();
            if (physicalPage == -1) {
                DEBUG('a', "No more physical pages available.\n");
                ASSERT(false);
            }
            pageTable[i].physicalPage = physicalPage;
          #else
            pageTable[i].physicalPage = -1;
          #endif
        #else
          pageTable[i].physicalPage = i;
        #endif
        #ifndef DEMAND_LOADING
          pageTable[i].valid        = true;
        #else
          pageTable[i].valid        = false;
        #endif
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
          // If the code segment was entirely on a separate page, we could
          // set its pages to be read-only.
    }



    #ifdef USER_PROGRAM
    char *mainMemory = machine->mainMemory;
    #ifndef DEMAND_LOADING
    
    for (unsigned i = 0; i < numPages; i++){
        memset(mainMemory + (pageTable[i].physicalPage * PAGE_SIZE), 0, PAGE_SIZE);
    }
    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe->GetCodeSize();
    uint32_t initDataSize = exe->GetInitDataSize();
    if (codeSize > 0) {
        uint32_t virtualAddr = exe->GetCodeAddr();
        DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
              virtualAddr, codeSize);
        #ifdef USER_PROGRAM
          uint32_t readBytes = 0;
          while (readBytes < codeSize) {
            int addr = TranlateAddress(virtualAddr + readBytes);
            int bytes = exe->ReadCodeBlock(&mainMemory[addr],
                                          1, readBytes);
            if (bytes == 0) {
                break;
            }
            readBytes += bytes;
          }
        #else
          exe->ReadCodeBlock(&mainMemory[virtualAddr], codeSize, 0);
        #endif
    }
    if (initDataSize > 0) {
        uint32_t virtualAddr = exe->GetInitDataAddr();
        DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",
              virtualAddr, initDataSize);
        #ifdef USER_PROGRAM
          uint32_t readBytes = 0;
          while (readBytes < initDataSize) {
            int addr = TranlateAddress(virtualAddr + readBytes);
            int bytes = exe->ReadDataBlock(&mainMemory[addr],
                                          1, readBytes);
            if (bytes == 0) {
                break;
            }
            readBytes += bytes;
          }
        #else
          exe->ReadDataBlock(&mainMemory[virtualAddr], initDataSize, 0);
        #endif
    }
    #endif
    #else
    memset(mainMemory, 0, size);
    #endif
}

#ifdef DEMAND_LOADING
void AddressSpace::LoadPage(int page){
    // exe = new Executable(executableFile);
    ASSERT(exe->CheckMagic());
    
    char *mainMemory = machine->mainMemory;
    #ifdef SWAP
    int physicalPage = pages->Find(page, pid);
    #else
    int physicalPage = pages->Find();
    #endif
    if (physicalPage == -1) {
        DEBUG('e', "No more physical pages available.\n");
        #ifdef SWAP
          physicalPage = PickVictim();
          int *toSwap = pages->getFrame(physicalPage);
          int vpn = toSwap[0];
          int spid = toSwap[1];
          threadTable->Get(spid)->space->Swap(physicalPage, vpn);
          pages->Mark(page, pid, physicalPage);
        #else
          ASSERT(false);
        #endif
    }


#ifdef SWAP
    if(swapMap->Test(page)){
      DEBUG('f', "Loading page %d from SWAP to physical page %d\n", page, physicalPage);
      diskSpace->ReadAt(&mainMemory[physicalPage * PAGE_SIZE], PAGE_SIZE, page * PAGE_SIZE);
      pageTable[page].physicalPage = physicalPage;
      pageTable[page].valid = true;
      stats->readFromSwap++;
    }else
#endif
    {
    pageTable[page].physicalPage = physicalPage;
    pageTable[page].valid = true;
    DEBUG('e', "Loading page %d to physical page %d\n", page, physicalPage);
    memset(mainMemory + (pageTable[page].physicalPage * PAGE_SIZE), 0, PAGE_SIZE);
    uint32_t virtualAddr = page * PAGE_SIZE;
    uint32_t readBytes = 0;
    uint32_t codeSize = exe->GetCodeSize();
    uint32_t codeAddress = exe->GetCodeAddr();
    uint32_t dataSize = exe->GetInitDataSize();
    uint32_t dataAddress = exe->GetInitDataAddr();
    if(virtualAddr < codeAddress + codeSize){
        while (readBytes < PAGE_SIZE) {
            int addr = TranlateAddress(virtualAddr + readBytes);
            uint32_t offset = readBytes + (virtualAddr - codeAddress);
            if (offset >= codeSize) {
                break;
            }
            int bytes = exe->ReadCodeBlock(&mainMemory[addr],
                                          1, offset);
            readBytes += bytes;
        }
    }
    DEBUG('e', "Loaded code segment\n");
    int codeCount = readBytes;
    readBytes = 0;
    DEBUG('e', "Data size: %d\n", dataSize);
    if(virtualAddr < dataAddress + dataSize){
      uint32_t offset = codeCount > 0 ? 0 : virtualAddr - dataAddress;
      while((readBytes + codeCount) < PAGE_SIZE){
        int addr = TranlateAddress(virtualAddr + codeCount + readBytes);
        if(offset + readBytes >= dataSize){
          break;
        }
        int bytes = exe->ReadDataBlock(&mainMemory[addr],
                                        1, offset + readBytes);
        readBytes += bytes;
      }
    }
    }
    
}
#ifdef SWAP
void AddressSpace::Swap(int physical, int vpn){
    if(diskSpace == nullptr){
        DEBUG('e', "Creating swap file for process %d\n", pid);
        char spaceName[10];
        sprintf(spaceName, "SWAP.%d", pid);
        ASSERT(fileSystem->Create(spaceName, numPages * PAGE_SIZE));
        ASSERT(diskSpace = fileSystem->Open(spaceName));
    }
    if(currentThread->space == this){
      TranslationEntry* tlb = machine->GetMMU()->tlb;
      for(unsigned i = 0; i < TLB_SIZE; i++){
        if(tlb[i].physicalPage == static_cast<unsigned>(physical) && tlb[i].valid){
          pageTable[vpn] = tlb[i];
          tlb[i].valid = false;
        }
      }
    }

    // if(pageTable[vpn].dirty || !swapMap->Test(vpn)){
      char* mainMemory = machine->mainMemory;
      DEBUG('f', "Writing page %d (physical address %d) to SWAP\n", vpn, physical);
      diskSpace->WriteAt(&mainMemory[physical * PAGE_SIZE], PAGE_SIZE, vpn * PAGE_SIZE);
      pageTable[vpn].valid = false;
      swapMap->Mark(vpn);
      stats->writeToSwap++;
    // }
}

int AddressSpace::PickVictim(){
  int r;
  #ifdef PRPOLICY_FIFO
    r = head;
    head = (head+1)%machine->GetNumPhysicalPages();
  #endif
  #ifdef PRPOLICY_CLOCK
    for(int ronda = 0; ronda < 4; ronda++){
      for(int i = head; i < head + machine->GetNumPhysicalPages(); i++){
          int* check = pages->getFrame(i % machine->GetNumPhysicalPages());
          int checkVPN = check[0];
          int checkPID = check[1];
          TranslationEntry *pageCandidate = &(threadTable->Get(checkPID)->space->pageTable[checkVPN]);
          switch (ronda){
            case 0:
              if(!pageCandidate->use && !pageCandidate->dirty){
                r = i % machine->GetNumPhysicalPages();
                head = (r + 1) % machine->GetNumPhysicalPages();
                return r;
              }
              break;
            case 1:
              if(!pageCandidate->use && pageCandidate->dirty){
                r = i % machine->GetNumPhysicalPages();
                head = (r + 1) % machine->GetNumPhysicalPages();
                return r;
              }else{
                pageCandidate->use = false;
              }
              break;
            case 2:
              if(!pageCandidate->use && !pageCandidate->dirty){
                r = i % machine->GetNumPhysicalPages();
                head = (r + 1) % machine->GetNumPhysicalPages();
                return r;
              }
              break;
            case 3:
              if(!pageCandidate->use && pageCandidate->dirty){
                r = i % machine->GetNumPhysicalPages();
                head = (r + 1) % machine->GetNumPhysicalPages();
                return r;
              }
              break;
          }
      }
    }
  #endif
  #ifdef PRPOLICY_RANDOM
    r = SystemDep::Random() % machine->GetNumPhysicalPages();
  #endif
  return r;
}

#endif

#endif
/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    #ifdef USER_PROGRAM
      for (unsigned i = 0; i < numPages; i++) {
          if(pageTable[i].valid)
            pages->Clear(pageTable[i].physicalPage);
      }
    #endif
    #ifdef SWAP
      if(diskSpace != nullptr){
        delete diskSpace;
      }
    #endif
    delete exe;
    delete [] pageTable;
}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++) {
        machine->WriteRegister(i, 0);
    }

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{
    #ifdef SWAP
      TranslationEntry* tlb = machine->GetMMU()->tlb;
      for(unsigned i = 0; i < TLB_SIZE; i++){
        if(tlb[i].valid){
          pageTable[tlb[i].virtualPage] = tlb[i];
          tlb[i].valid = false;
        }
      }
    #endif
}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
  #ifndef USE_TLB
    DEBUG('a', "Not using TLB.\n");
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
  #else
    for(unsigned i = 0; i < TLB_SIZE; i++){
      machine->GetMMU()->tlb[i].valid = false;
    }
  #endif
}

TranslationEntry
AddressSpace::GetPageTable(int addr){
    return pageTable[addr];
}

