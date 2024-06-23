#include "file_data.hh"
#include "open_file.hh"
#include "file_system.hh"
#include "threads/system.hh"
#include "threads/condition.hh"


FileData::FileData(OpenFile *_openFile)
{
    openFile = _openFile;
    opens = 1;
    readers = 0;
    writers = 0;
    writing = false;
    lock = new Lock(openFile->getName());
    condition = new Condition(openFile->getName(), lock);
    deleted = false;
}

FileData::~FileData()
{
    delete openFile;
    delete lock;
    delete condition;
}