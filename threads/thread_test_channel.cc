#include "thread_test_channel.hh"
#include "channel.hh"
#include "system.hh"

#include <stdio.h>

Channel *channel = new Channel("test");
int sefini[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static void Sender(void *n) {
    for (int i = 0; i < 10; i++) {
        channel->Send(i);
        printf("Sending: %d\n", i);
    }
    *(int*)n = 1;
}

static void Receiver(void *n) {
    for (int i = 0; i < 10; i++) {
        int message;
        channel->Receive(&message);
        printf("Received: %d\n", message);
    }
    *(int*)n = 1;
}

void ThreadTestChannel() {
    Thread *sender1 = new Thread("Sender1");
    Thread *sender2 = new Thread("Sender2");
    Thread *sender3 = new Thread("Sender3");
    Thread *sender4 = new Thread("Sender4");
    Thread *sender5 = new Thread("Sender5");
    Thread *reciever1 = new Thread("Reciever1");
    Thread *reciever2 = new Thread("Reciever2");
    Thread *reciever3 = new Thread("Reciever3");
    Thread *reciever4 = new Thread("Reciever4");
    Thread *reciever5 = new Thread("Reciever5");

    sender1->Fork(Sender, sefini + 0);
    sender2->Fork(Sender, sefini + 1);
    sender3->Fork(Sender, sefini + 2);
    sender4->Fork(Sender, sefini + 3);
    sender5->Fork(Sender, sefini + 4);
    reciever1->Fork(Receiver, sefini + 5);
    reciever2->Fork(Receiver, sefini + 6);
    reciever3->Fork(Receiver, sefini + 7);
    reciever4->Fork(Receiver, sefini + 8);
    reciever5->Fork(Receiver, sefini + 9);

    while(!sefini[0] || !sefini[1] || !sefini[2] || !sefini[3] || !sefini[4] || !sefini[5] || !sefini[6] || !sefini[7] || !sefini[8] || !sefini[9]) currentThread->Yield();
}