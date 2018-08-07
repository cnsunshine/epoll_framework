#include <iostream>
#include "Epoll.cpp" //must use cpp

struct message {
    int a;
    int b;
};

//req.data请使用栈上数据
void solveRecv(const SocketData<message> res, SocketData<message> *const req) {
    for (int i = 0; i < res.size; ++i) {
        std::cout << (res.data + i)->a << std::endl;
        std::cout << (res.data + i)->b << std::endl;
    }
    req->fd = res.fd;
    req->size = res.size;
    req->data = res.data;
}

int main() {
    Epoll<message> epollServer(6666, 100, 20, 20);
    epollServer.bindRecvFunc(solveRecv);
    epollServer.init();
    return 0;
}