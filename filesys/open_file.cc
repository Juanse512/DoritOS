/// Routines to manage an open Nachos file.  As in UNIX, a file must be open
/// before we can read or write to it.  Once we are all done, we can close it
/// (in Nachos, by deleting the `OpenFile` data structure).
///
/// Also as in UNIX, for convenience, we keep the file header in memory while
/// the file is open.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "open_file.hh"
#include "file_header.hh"
#include "threads/system.hh"
#include "file_data.hh"
#include <string.h>


/// Open a Nachos file for reading and writing.  Bring the file header into
/// memory while the file is open.
///
/// * `sector` is the location on disk of the file header for this file.
OpenFile::OpenFile(int sector, const char *_name)
{
    hdr = new FileHeader;
    hdr->FetchFrom(sector);
    // seekPosition = 0;
    hdrSector = sector;
    seekPositionList = std::map<int, unsigned>();
    seekPositionList.insert(std::pair<int, unsigned>(currentThread->GetId(), 0));
    name = _name;
}

/// Close a Nachos file, de-allocating any in-memory data structures.
OpenFile::~OpenFile()
{
    delete hdr;
}

/// Change the current location within the open file -- the point at which
/// the next `Read` or `Write` will start from.
///
/// * `position` is the location within the file for the next `Read`/`Write`.
void
OpenFile::Seek(unsigned position)
{
    // seekPosition = position;
    seekPositionList[currentThread->GetId()] = position;
}

/// OpenFile::Read/Write
///
/// Read/write a portion of a file, starting from `seekPosition`.  Return the
/// number of bytes actually written or read, and as a side effect, increment
/// the current position within the file.
///
/// Implemented using the more primitive `ReadAt`/`WriteAt`.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.

int
OpenFile::Read(char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);
    
    int result = ReadAt(into, numBytes, seekPositionList[currentThread->GetId()]);
    seekPositionList[currentThread->GetId()] = result+seekPositionList[currentThread->GetId()];
    return result;
}

int
OpenFile::Write(const char *into, unsigned numBytes)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    int result = WriteAt(into, numBytes, seekPositionList[currentThread->GetId()]);
    seekPositionList[currentThread->GetId()] = result+seekPositionList[currentThread->GetId()];
    return result;
}

/// OpenFile::ReadAt/WriteAt
///
/// Read/write a portion of a file, starting at `position`.  Return the
/// number of bytes actually written or read, but has no side effects (except
/// that `Write` modifies the file, of course).
///
/// There is no guarantee the request starts or ends on an even disk sector
/// boundary; however the disk only knows how to read/write a whole disk
/// sector at a time.  Thus:
///
/// For ReadAt:
///     We read in all of the full or partial sectors that are part of the
///     request, but we only copy the part we are interested in.
/// For WriteAt:
///     We must first read in any sectors that will be partially written, so
///     that we do not overwrite the unmodified portion.  We then copy in the
///     data that will be modified, and write back all the full or partial
///     sectors that are part of the request.
///
/// * `into` is the buffer to contain the data to be read from disk.
/// * `from` is the buffer containing the data to be written to disk.
/// * `numBytes` is the number of bytes to transfer.
/// * `position` is the offset within the file of the first byte to be
///   read/written.

int
OpenFile::ReadAt(char *into, unsigned numBytes, unsigned position)
{
    return ReadAt(into, numBytes, position, false);
}

int
OpenFile::ReadAt(char *into, unsigned numBytes, unsigned position, bool system)
{
    ASSERT(into != nullptr);
    ASSERT(numBytes > 0);

    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    char *buf;

    if (position >= fileLength) {
        return 0;  // Check request.
    }
    if (position + numBytes > fileLength) {
        numBytes = fileLength - position;
    }
  
    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors = 1 + lastSector - firstSector;

    FileData *openFileData;
    if(!system){
        openFilesLock->Acquire();
        DEBUG('f', "Requested FileData on sector %u.\n", hdrSector);
        openFileData = openFiles->find(hdrSector)->second;
        openFilesLock->Release();

        openFileData->lock->Acquire();
        while(openFileData->writers > 0 || openFileData->writing) 
            openFileData->condition->Wait();
        
        openFileData->readers++;
        openFileData->lock->Release();
    }


    // Read in all the full and partial sectors that we need.
    buf = new char [numSectors * SECTOR_SIZE];
    for (unsigned i = firstSector; i <= lastSector; i++) {
        synchDisk->ReadSector(hdr->ByteToSector(i * SECTOR_SIZE),
                              &buf[(i - firstSector) * SECTOR_SIZE]);
    }

    // Copy the part we want.
    memcpy(into, &buf[position - firstSector * SECTOR_SIZE], numBytes);

    if(!system){
        openFileData->lock->Acquire();
        openFileData->readers--;
        if(openFileData->readers == 0)
            openFileData->condition->Broadcast();

        openFileData->lock->Release();
    }
    delete [] buf;
    return numBytes;
}

int OpenFile::WriteAt(const char *from, unsigned numBytes, unsigned position)
{
    return WriteAt(from, numBytes, position, false);
}

int
OpenFile::WriteAt(const char *from, unsigned numBytes, unsigned position, bool system)
{
    ASSERT(from != nullptr);
    ASSERT(numBytes > 0);
    unsigned fileLength = hdr->FileLength();
    unsigned firstSector, lastSector, numSectors;
    bool firstAligned, lastAligned;
    char *buf;
    if (position + numBytes > fileLength) {
        unsigned toAdd = position + numBytes - fileLength;
        // hdr->Extend(hdr, toAdd);
        if(!fileSystem->ExtendFile(hdr, toAdd + hdr->GetRaw()->numBytes))
            return 0;
        DEBUG('f', "File extended.\n");
        fileLength = hdr->FileLength();
        hdr->WriteBack(hdrSector);
    }
    if (position + numBytes > fileLength) {
        numBytes = fileLength - position;
    }

    firstSector = DivRoundDown(position, SECTOR_SIZE);
    lastSector  = DivRoundDown(position + numBytes - 1, SECTOR_SIZE);
    numSectors  = 1 + lastSector - firstSector;

    buf = new char [numSectors * SECTOR_SIZE];

    firstAligned = position == firstSector * SECTOR_SIZE;
    lastAligned  = position + numBytes == (lastSector + 1) * SECTOR_SIZE;

    // Read in first and last sector, if they are to be partially modified.
    if (!firstAligned) {
        ReadAt(buf, SECTOR_SIZE, firstSector * SECTOR_SIZE, system);
    }
    if (!lastAligned && (firstSector != lastSector || firstAligned)) {
        ReadAt(&buf[(lastSector - firstSector) * SECTOR_SIZE], SECTOR_SIZE, lastSector * SECTOR_SIZE, system);
    }

    // Copy in the bytes we want to change.
    memcpy(&buf[position - firstSector * SECTOR_SIZE], from, numBytes);
    FileData *openFileData;
    if(!system){
        openFilesLock->Acquire();
        openFileData = openFiles->find(hdrSector)->second;
        openFilesLock->Release();
        openFileData->lock->Acquire();
        openFileData->writers++;
        while(openFileData->writing || openFileData->readers > 0) 
            openFileData->condition->Wait();
        openFileData->writers--;
        openFileData->writing = true;
        openFileData->lock->Release();
    }

    // Write modified sectors back.
    for (unsigned i = firstSector; i <= lastSector; i++) {
        synchDisk->WriteSector(hdr->ByteToSector(i * SECTOR_SIZE),
                               &buf[(i - firstSector) * SECTOR_SIZE]);
    }
    if(!system){
        openFileData->lock->Acquire();
        openFileData->writing = false;
        openFileData->condition->Broadcast();
        openFileData->lock->Release();
    }
    delete [] buf;
    return numBytes;
}

/// Return the number of bytes in the file.
unsigned
OpenFile::Length() const
{
    return hdr->FileLength();
}
