#include "channel.hh"

// Implement the member functions of the Channel class here

// Constructor
Channel::Channel(const char* debugName) {
    name = debugName;
    sending = new Semaphore("sending", 1);
    receiving = new Semaphore("receiving",0);
    received = new Semaphore("bufferLock", 0);
}

// Destructor
Channel::~Channel() {
    delete sending;
    delete receiving;
    delete received;
    delete name;
}

// Method to send a message through the channel     
void Channel::Send(int message) {
    sending->P();
    buffer = message;
    receiving->V();
    received->P();
    sending->V();
}

// Method to receive a message from the channel
void Channel::Receive(int* message) {
    receiving->P();
    *message = buffer;
    received->V();
}

const char* Channel::GetName() const {
    return name;
}