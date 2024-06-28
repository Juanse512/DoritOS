/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>


/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the bit map of free disk sectors.

FileHeader::FileHeader()
{
    raw.numBytes = 0;
    raw.numSectors = 0;
    raw.indirectSector = 0;
    for (unsigned i = 0; i < NUM_DIRECT; i++) {
        raw.dataSectors[i] = 0;
    }
    indirectHeader = nullptr;
}

FileHeader::~FileHeader()
{
    if (indirectHeader != nullptr) {
        delete indirectHeader;
    }
}

bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize)
{
    ASSERT(freeMap != nullptr);

    if (fileSize > MAX_FILE_SIZE) {
        return false;
    }

    raw.numBytes = fileSize;
    raw.numSectors = DivRoundUp(fileSize, SECTOR_SIZE);
    if (freeMap->CountClear() < raw.numSectors) {
        return false;  // Not enough space.
    }

    unsigned numDirect = raw.numSectors > NUM_DIRECT ? NUM_DIRECT : raw.numSectors;
    unsigned i;
    for (i = 0; i < numDirect; i++) {
        raw.dataSectors[i] = freeMap->Find();
    }

    if (raw.numSectors > NUM_DIRECT) {
        if (indirectHeader == nullptr) {
            indirectHeader = new FileHeader();

            FileHeader *temp = indirectHeader;
            raw.indirectSector = freeMap->Find();
            for(; i < raw.numSectors; i += NUM_DIRECT){
                raw.numBytes = fileSize;
                raw.numSectors = raw.numSectors - i;
                unsigned rest = raw.numSectors > NUM_DIRECT ? NUM_DIRECT : raw.numSectors;
                for (unsigned j = 0; j < rest; j++) {
                    temp->raw.dataSectors[j] = freeMap->Find();
                }

                if(i + NUM_DIRECT < raw.numSectors){
                    temp->raw.indirectSector = freeMap->Find();
                    temp->indirectHeader = new FileHeader();
                    temp = temp->indirectHeader;
                }

            }
        }
    }
    
    return true;
}

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void
FileHeader::Deallocate(Bitmap *freeMap)
{
    ASSERT(freeMap != nullptr);

    for (unsigned i = 0; i < raw.numSectors; i++) {
        ASSERT(freeMap->Test(raw.dataSectors[i]));  // ought to be marked!
        freeMap->Clear(raw.dataSectors[i]);
    }
    if(indirectHeader)
        indirectHeader->Deallocate(freeMap);
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
    synchDisk->ReadSector(sector, (char *) &raw);
    if(raw.numSectors > NUM_DIRECT){
        indirectHeader = new FileHeader();
        indirectHeader->FetchFrom(raw.indirectSector);
    }
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
    DEBUG('f', "Writing raw: %p into sector: %u\n", &raw, sector);
    synchDisk->WriteSector(sector, (char *) &raw);
    DEBUG('f', "indirect table: %p\n", indirectHeader);
    if(indirectHeader)
        indirectHeader->WriteBack(raw.indirectSector);
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset)
{
    if (offset >= SECTOR_SIZE * NUM_DIRECT) {
        return indirectHeader->ByteToSector(offset - SECTOR_SIZE * NUM_DIRECT);
    }
    return raw.dataSectors[offset / SECTOR_SIZE];
}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
    return raw.numBytes;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
    char *data = new char [SECTOR_SIZE];

    if (title == nullptr) {
        printf("File header:\n");
    } else {
        printf("%s file header:\n", title);
    }

    printf("    size: %u bytes\n"
           "    block indexes: ",
           raw.numBytes);

    for (unsigned i = 0; i < raw.numSectors; i++) {
        printf("%u ", raw.dataSectors[i]);
    }
    printf("\n");

    for (unsigned i = 0, k = 0; i < raw.numSectors; i++) {
        printf("    contents of block %u:\n", raw.dataSectors[i]);
        synchDisk->ReadSector(raw.dataSectors[i], data);
        for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++) {
            if (isprint(data[j])) {
                printf("%c", data[j]);
            } else {
                printf("\\%X", (unsigned char) data[j]);
            }
        }
        printf("\n");
    }
    delete [] data;
}

const RawFileHeader *
FileHeader::GetRaw() const
{
    return &raw;
}

bool 
FileHeader::Extend(Bitmap *freeMap, unsigned bytes)
{
    unsigned newSectors = DivRoundUp(bytes + raw.numBytes, SECTOR_SIZE) - raw.numSectors;
    unsigned currentSectors = raw.numSectors;
    // if(newSectors <= currentSectors){
    //     DEBUG('f', "File already has enough sectors %u %u.\n", newSectors, currentSectors);
    //     return false;
    // }
    DEBUG('f', "Extending file %u sectors, available %u.\n", newSectors, freeMap->CountClear());
    if(freeMap->CountClear() < newSectors){
        DEBUG('f', "Not enough free sectors.\n");
        return false;
    }

    unsigned i = 0;
    if(newSectors + currentSectors <= NUM_DIRECT){
        for (i = currentSectors; i < newSectors + currentSectors; i++) {
            raw.dataSectors[i] = freeMap->Find();
        }
        raw.numBytes += bytes;
        raw.numSectors += newSectors;
    }

    if (newSectors + currentSectors > NUM_DIRECT) {
        if (indirectHeader == nullptr) {
            unsigned numHeaders = DivRoundUp(newSectors, NUM_DIRECT);
            DEBUG('f', "Num headers: %u Available: %d\n", numHeaders, freeMap->CountClear());
            if(freeMap->CountClear() < numHeaders){
                DEBUG('f', "Not enough free sectors.\n");
                return false;
            }
            raw.indirectSector = freeMap->Find();
            indirectHeader = new FileHeader();

            FileHeader *temp = indirectHeader;
            for(; i < newSectors; i += NUM_DIRECT){

                unsigned rest = (newSectors - i) > NUM_DIRECT ? NUM_DIRECT : newSectors - i;
                for (unsigned j = 0; j < rest; j++) {
                    temp->raw.dataSectors[j] = freeMap->Find();
                }
                temp->raw.numBytes = bytes - (i * SECTOR_SIZE);
                temp->raw.numSectors = newSectors - i;
                if(i + NUM_DIRECT < raw.numSectors){
                    temp->raw.indirectSector = freeMap->Find();
                    temp->indirectHeader = new FileHeader();
                    temp = temp->indirectHeader;
                }

            }
        }else{
            if(!indirectHeader->Extend(freeMap, bytes)){
                DEBUG('f', "Indirect table not extended.\n");
                return false;
            }
            raw.numBytes += bytes;
            raw.numSectors += newSectors;
        }
    }
    
    return true;
}