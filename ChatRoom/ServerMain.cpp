#include "EventLoop.h"
#include "ChatRoom.h"
int main(){
    ChatRoom room("192.168.253.130",8888);
    room.start();
    return 0;
}