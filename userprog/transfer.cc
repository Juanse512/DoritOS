/// Copyright (c) 2019-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "lib/utility.hh"
#include "threads/system.hh"


void ReadBufferFromUser(int userAddress, char *outBuffer,
                        unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outBuffer != nullptr);
    ASSERT(byteCount != 0);

    for (unsigned i = 0; i < byteCount; i++) {
        int temp;
        bool success = false;
        for(int j = 0; j < READLIMIT; j++){
            if((success = machine->ReadMem(userAddress, 1, &temp))){
                userAddress++;
                break;
            }
        }
        ASSERT(success);
        *outBuffer++ = (unsigned char) temp;
    }
}

bool ReadStringFromUser(int userAddress, char *outString,
                        unsigned maxByteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outString != nullptr);
    ASSERT(maxByteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        bool success = false;
        for(int i=0; i<READLIMIT; i++){
            if((success = machine->ReadMem(userAddress, 1, &temp))){
                count++;
                userAddress++;
                *outString = (unsigned char) temp;
                break;
            }
        }
        ASSERT(success);
    } while (*outString++ != '\0' && count < maxByteCount);

    return *(outString - 1) == '\0';
}

void WriteBufferToUser(const char *buffer, int userAddress,
                       unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(buffer != nullptr);
    ASSERT(byteCount != 0);

    for (unsigned i = 0; i < byteCount; i++) {
        bool success = false;
        for(int j = 0; j < READLIMIT; j++){
            if((success = machine->WriteMem(userAddress, 1, (int) *(buffer + i)))) {
                userAddress++;
                break;
            }
        }
        ASSERT(success);
    }
}

void WriteStringToUser(const char *string, int userAddress)
{
    ASSERT(userAddress != 0);
    ASSERT(string != nullptr);

    do {
        bool success = false;
        for(int i=0; i<READLIMIT; i++){
            if((success=machine->WriteMem(userAddress, 1, (int) *string))) {
                userAddress++;
                break;
            }
        }
        ASSERT(success);
    } while (*string++ != '\0');
}
