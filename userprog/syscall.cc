#include "syscall.h"
#include <map>
#include "filesys/open_file.hh"
#include "filesys/file_system.hh"
#include "lib/table.hh"
#include "threads/system.hh"
Table<OpenFile*> *openFiles = new Table<OpenFile*>();

std::map<OpenFileId, int> *fileDescriptors = new std::map<OpenFileId, int>();

int openFileIdx = 2;

FileSystem *fileSystem = new FileSystem(true); 

int Create(const char *name) {
    ASSERT(name != NULL);
    return fileSystem->Create(name, 0);
}

int Remove(const char *name) {
    ASSERT(name != NULL);
    return fileSystem->Remove(name);
}

OpenFileId Open(const char *name) {
    ASSERT(name != NULL);
    OpenFile *file = fileSystem->Open(name);
    if (file == NULL) {
        return -1;
    }
    int fd = openFiles->Add(file);
    fileDescriptors->insert(std::pair<int, int>(openFileIdx, fd));
    return openFileIdx++;
}

int Write(const char *buffer, int size, OpenFileId id) {
    ASSERT(buffer != NULL && size >= 0);
    if(id == CONSOLE_INPUT){
        // me mataste
    }
    if(id == CONSOLE_OUTPUT){
        synchConsole->WriteBuffer(buffer, size);
        return size;
    }  
    int fd = fileDescriptors->at(id);
    OpenFile *file = openFiles->Get(fd);
    if (file == NULL) {
        return -1;
    }
    return file->Write(buffer, size);
}

int Read(char *buffer, int size, OpenFileId id) {
    if(id == CONSOLE_OUTPUT){
        // me mataste
    }
    if(id == CONSOLE_INPUT){
        synchConsole->ReadBuffer(buffer, size);
        return size;
    }
    ASSERT(buffer != NULL && size >= 0);
    int fd = fileDescriptors->at(id);
    OpenFile *file = openFiles->Get(fd);
    if (file == NULL) {
        return -1;
    }
    return file->Read(buffer, size);
}

int Close(OpenFileId id) {
    int fd = fileDescriptors->at(id);
    OpenFile *file = openFiles->Get(fd);
    if (file == NULL) {
        return -1;
    }
    openFiles->Remove(fd);
    delete file;
    return 0;
}
