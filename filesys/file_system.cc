/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "directory.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"
#include "threads/system.hh"
#include "file_data.hh"
#include <stdio.h>
#include <string.h>
#include "lib/utility.hh"
#include "filesys/directory.hh"
/// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;

/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory();
        FileHeader *mapH    = new FileHeader;
        FileHeader *dirH    = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE));

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile   = new OpenFile(FREE_MAP_SECTOR, "mapFile");
        directoryFile = new OpenFile(DIRECTORY_SECTOR, "dirFile");
        rootDirectoryFile = new OpenFile(DIRECTORY_SECTOR, "root");
        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        dir->WriteBack(directoryFile);

        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();

            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile   = new OpenFile(FREE_MAP_SECTOR, "mapFile");
        directoryFile = new OpenFile(DIRECTORY_SECTOR, "dirFile");
    }
}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}


Lock * getOpenDirLock(OpenFile * dir) {
    int sector = dir->GetSector();
    std::pair<int, Lock*> * lock = openDirectoriesLocks->find(sector);
    if(lock == openDirectoriesLocks->end()) {
        lock = new std::pair<int, Lock*>(sector, new Lock("Open Directory Lock"));
        openDirectoriesLocks->insert(lock);
    }
    return lock->second;
}
/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
/// * `initialSize` is the size of file to be created.
bool
FileSystem::CreateGenericAtomic(const char *name, unsigned initialSize, bool isDir){
    ASSERT(name != nullptr);
    ASSERT(initialSize < MAX_FILE_SIZE);

    DEBUG('f', "Creating file %s, size %u\n", name, initialSize);

    OpenFile *directoryFile = currentThread->GetDirectory();

    Directory *dir = new Directory();
    Lock *lock = getOpenDirLock(directoryFile);
    lock->Acquire();
    dir->FetchFrom(directoryFile);


    bool success;

    if (dir->Find(name) != -1) {
        lock->Release();
        return false;  // File is already in directory.
    } else {

        freeMapLock->Acquire();
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);
        int sector = freeMap->Find();
          // Find a sector to hold the file header.
        freeMap->WriteBack(freeMapFile);  // Save the changed bitmap to disk.
        freeMapLock->release();

        if (sector == -1) {
            lock->Release();
            return false;  // No free block for file header.
        } else {
            dir->Add(name, sector, isDir);

            FileHeader *h = new FileHeader;
            h->WriteBack(sector);
            // success = h->Allocate(freeMap, initialSize);
              // Fails if no space on disk for data.
            if(isDir){
                OpenFile *newDirFile = new OpenFile(sector, name);
                Directory *newDir = new Directory(NUM_DIR_ENTRIES, directoryFile->GetSector(), sector);
                newDir->WriteBack(newDirFile);
                delete newDir;
                delete newDirFile;
            }            

            dir->WriteBack(directoryFile);
            delete h;
        }
    }
    lock->Release();
    delete dir;
    return success;
}
bool
FileSystem::CreateGeneric(const char *name, unsigned initialSize, bool isDir){
    char path[FILE_NAME_MAX_LEN];
    const char *fileName = filepath(path, name);
    if(strcmp("", path) == 0){
        return CreateGenericAtomic(fileName, initialSize, isDir);
    }else{
        OpenFile *dirBackup = new OpenFile(currentThread->GetDirectory()->GetSector(), currentThread->GetDirectory()->getName());
        bool change = ChangeDirectory(path);
        if(!change){
            delete dirBackup;
            return false;
        }
        bool res = CreateGenericAtomic(fileName, initialSize, isDir);
        currentThread->setDirectory(dirBackup);
        return res;
    }

}
bool FileSystem::ChangeDirectory(char * path){
    OpenFile *dir = currentThread->GetDirectory();
    Directory *directory = new Directory();
    directory->FetchFrom(dir);
    char *token = strtok(path, "/");
    while(token != nullptr){
        if(strcmp(token, "..") == 0){
            int parentSector = directory->GetParentSector();
            if(parentSector == -1){
                return false;
            }
            delete directory;
            delete dir;
            dir = new OpenFile(parentSector, "dirFile");
            directory = new Directory();
            directory->FetchFrom(dir);
        }else{
            int sector = directory->Find(token);
            if(sector == -1){
                return false;
            }
            delete directory;
            delete dir;
            dir = new OpenFile(sector, "dirFile");
            directory = new Directory(NUM_DIR_ENTRIES);
            directory->FetchFrom(dir);
        }
        token = strtok(nullptr, "/");
    }
    currentThread->setDirectory(dir);
    return true;
}

bool
FileSystem::Create(const char *name, unsigned initialSize)
{
    ASSERT(name != nullptr);
    ASSERT(initialSize < MAX_FILE_SIZE);

    return CreateGeneric(name, initialSize, false);
}

bool
FileSystem::CreateDir(const char *name)
{
    ASSERT(name != nullptr);
    return CreateGeneric(name, 0, true);
    
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile*
FileSystem::OpenAtomic(const char *name){
    ASSERT(name != nullptr);
    OpenFile *directoryFile = currentThread->GetDirectory();
    Directory *dir = new Directory();
    Lock *lock = getOpenDirLock(directoryFile);
    lock->Acquire();
    dir->FetchFrom(directoryFile);
    OpenFile  *openFile = nullptr;
    FileData  *openFileData = nullptr;
    DEBUG('f', "Opening file %s\n", name);
    dir->FetchFrom(directoryFile);
    int sector = dir->Find(name);
    if(dir->isDirectory(sector)){
        lock->Release();
        delete dir;
        return nullptr;
    }
    if (sector >= 0) {
        openFilesLock->Acquire();
        if(openFiles->find(sector) == openFiles->end()){
            openFile = new OpenFile(sector, name);
            openFileData = new FileData(openFile);
            openFiles->insert(std::pair<int, FileData*>(sector, openFileData));
        } else {
            openFileData = openFiles->find(sector)->second;
            if(openFileData->deleted){
                DEBUG('f', "File %s was deleted\n", name);
                lock->Release();
                openFilesLock->Release();
                return nullptr;
            }
            openFile = openFileData->openFile;
            openFileData->opens++;
            openFile->addSeekPosition(currentThread->GetId(), 0);
        }
        openFilesLock->Release();
        // openFile = new OpenFile(sector);  // `name` was found in directory.
    }
    lock->Release();
    delete dir;
    return openFile;  // Return null if not found.
}


OpenFile *
FileSystem::Open(const char *name)
{
    ASSERT(name != nullptr);

    Directory *dir = new Directory();
    char path[FILE_NAME_MAX_LEN];
    const char *fileName = filepath(path, name);
    if(strcmp("", path) == 0){
        OpenFile *openFile = OpenAtomic(fileName);
        delete dir;
        return openFile;
    }
    OpenFile * backup = new OpenFile(currentThread->GetDirectory()->GetSector(), currentThread->GetDirectory()->getName());
    bool change = ChangeDirectory(path);
    if(!change){
        delete backup;
        delete dir;
        return nullptr;
    }
    OpenFile *openFile = OpenAtomic(fileName);
    currentThread->setDirectory(backup);
    delete dir;
    return openFile;
}

void 
FileSystem::Close(OpenFile *openFile){
    openFilesLock->Acquire();
    FileData *openFileData = openFiles->find(openFile->GetSector())->second;

    openFileData->opens--;
    if(openFileData->opens == 0){
        openFiles->erase(openFile->GetSector());
        delete openFileData;
    }
    openFilesLock->Release();
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.

bool
FileSystem::Remove(const char *name)
{
    ASSERT(name != nullptr);

    OpenFile *dirFile = currentThread->GetDirectory();

    Directory *dir = new Directory();
    Lock * lock = getOpenDirLock(dirFile);
    lock->Acquire();
    dir->FetchFrom(dirFile);
    // dir->GetLock()->Acquire();
    int sector = dir->Find(name);
    if (sector == -1) {
       delete dir;
       return false;  // file not found
       lock->Release();
    }
    FileData *openFileData;
    if(!dir->isDirectory(sector)){

        openFilesLock->Acquire();
        if(openFiles->find(sector) != openFiles->end()){
            openFileData = openFiles->find(sector)->second;
            openFileData->deleted = true;
            openFilesLock->Release();
            lock->Release();
            return true;
        }else{
            FileHeader *fileH = new FileHeader;
            fileH->FetchFrom(sector);

            Bitmap *freeMap = new Bitmap(NUM_SECTORS);
            freeMapLock->Acquire();
            freeMap->FetchFrom(freeMapFile);

            fileH->Deallocate(freeMap);  // Remove data blocks.
            freeMap->Clear(sector);      // Remove header block.
            dir->Remove(name);

            freeMap->WriteBack(freeMapFile);  // Flush to disk.
            dir->WriteBack(dirFile);    // Flush to disk.
            freeMapLock->Release();
            dir->Remove(name);
            dir->WriteBack(dirFile);
            delete fileH;
            delete dir;
            delete freeMap;

            openFilesLock->Release();
            lock->Release();
            return true;
        }
    }else{
        OpenFile *dirFile = new OpenFile(sector, name);
        Directory *dir = new Directory();
        Lock * lock = getOpenDirLock(dirFile);
        lock->Acquire();
        dir->FetchFrom(dirFile);
        bool empty = dir->isEmpty();
        if(!empty){
            lock->Release();
            delete dir;
            delete dirFile;
            return false;
        }
        lock->Release();
        delete dir;
        delete dirFile;
        openFilesLock->Acquire();
        FileHeader *fileH = new FileHeader;
        fileH->FetchFrom(sector);

        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMapLock->Acquire();
        freeMap->FetchFrom(freeMapFile);

        fileH->Deallocate(freeMap);  // Remove data blocks.
        freeMap->Clear(sector);      // Remove header block.
        dir->Remove(name);

        freeMap->WriteBack(freeMapFile);  // Flush to disk.
        dir->WriteBack(dirFile);    // Flush to disk.
        freeMapLock->Release();
        dir->Remove(name);
        dir->WriteBack(dirFile);
        delete fileH;
        delete dir;
        delete freeMap;

        openFilesLock->Release();
        lock->Release();
        return true;
    }
}

/// List all the files in the file system directory.
void
FileSystem::List()
{
    Directory *dir = new Directory();

    dir->FetchFrom(directoryFile);
    dir->List();
    delete dir;
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value) {
        DEBUG('f', "Error: %s\n", message);
    }
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    if (CheckForError(sector < NUM_SECTORS,
                      "sector number too big.  Skipping bitmap check.")) {
        return true;
    }
    return CheckForError(AddToShadowBitmap(sector, shadowMap),
                         "sector number already used.");
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "sector count not compatible with file size.");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
                           "too many blocks.");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char *knownNames[NUM_DIR_ENTRIES];

    for (unsigned i = 0; i < NUM_DIR_ENTRIES; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "bad bitmap header: wrong file size.");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "bad bitmap header: wrong number of sectors.");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory();
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitH    = new FileHeader;
    FileHeader *dirH    = new FileHeader;
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory();

    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    dir->FetchFrom(directoryFile);
    dir->Print();
    printf("--------------------------------\n");

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}

bool 
FileSystem::ExtendFile(FileHeader *openFile, unsigned bytes){
    openFilesLock->Acquire();
    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    // Tomo el bitmap de espacios libres
    freeMap->FetchFrom(freeMapFile);
    bool res = openFile->Extend(freeMap, bytes);
    // Escribo los cambios del bitmap en el disco
    freeMap->WriteBack(freeMapFile);
    openFilesLock->Release();
    delete freeMap;
    return res;
}