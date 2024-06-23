#ifndef FILESYS_STUB
#ifndef NACHOS_FILESYS_FILEDATA__HH
#define NACHOS_FILESYS_FILEDATA__HH
#include "open_file.hh"
#include "threads/lock.hh"
#include "threads/condition.hh"


class FileData {
    public:
        FileData(OpenFile *_openFile);
        ~FileData();

        OpenFile *openFile;
        unsigned int opens;
        unsigned int readers;
        unsigned int writers;
        bool writing;
        Lock *lock;
        Condition *condition;
        bool deleted;
};
#endif
#endif