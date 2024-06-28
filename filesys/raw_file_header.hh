/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_RAWFILEHEADER__HH
#define NACHOS_FILESYS_RAWFILEHEADER__HH


#include "machine/disk.hh"


static const unsigned NUM_DIRECT
  = (SECTOR_SIZE / sizeof(int)) - 3; // Restamos 3 porque es lo que ocupan numBytes, numSectors y indirectSector
const unsigned MAX_FILE_SIZE = (NUM_SECTORS - 2) * SECTOR_SIZE; //-2 por bitmap y directorio

struct RawFileHeader {
    unsigned numBytes;  ///< Number of bytes in the file.
    unsigned numSectors;  ///< Number of data sectors in the file.
    unsigned dataSectors[NUM_DIRECT];  ///< Disk sector numbers for each data
                                       ///< block in the file.
    unsigned indirectSector;  ///< Disk sector number for the indirect block.
};

// 0 0 0 0 0 0
// 32 3 - 5 7 8
// sector[0] = numbytes
// sector[1] = numsectors
// sector[2] = indirectSector
// sector[3 -> sizeof(127)] = sectores del archivo

#endif
