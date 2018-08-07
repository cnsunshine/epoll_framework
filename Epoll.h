//
// Created by sunshine on 18-8-6.
//

#ifndef INC_2018080601EPOLL_FRAMEWORK_EPOLL_H
#define INC_2018080601EPOLL_FRAMEWORK_EPOLL_H

#include <ctime>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <map>
#include <vector>

struct MessagePackageHeader {
    int size; //数据量，这里不需要长度，一是因为tcp在传输层会自动封装，二是编译期需要确定类型
    time_t timestamp;
};

struct ClientInfo {
    sockaddr_in sockAddr;
    time_t loginTime;
    time_t lastAliveTime;
};

template<typename T>
struct SocketData {
    int fd;
    int size;
    T *data;
};

template<typename T>
class Epoll {
private:
    typedef void (*ReadFunc)(const SocketData<T> req, SocketData<T> *const res);

public:
    Epoll(int ServerPort, int EpollSize, int Timeout, int LISTENQ = 20);

    ~Epoll();

    void init();

    void bindRecvFunc(ReadFunc readFunc);

    ssize_t sendData(SocketData<T> response);

private:
    int ServerPort;
    int EpollSize;
    int Timeout;
    int LISTENQ;

    int epollFd, listenFd, connFd;
    socklen_t clientLen;
    MessagePackageHeader msgPkgHead;
    int nfds;
    epoll_event event, events[20];
    //
    std::map<int, ClientInfo> onlineList; //在线列表
    std::map<int, T> sendMessagePackageList; //待发送消息列表
    ReadFunc readFunc;

    //设置fd非阻塞
    void setNonBlocking(int sock);

    void s_epoll_create(int size);

    void s_socket();

    int s_epoll_ctl(int __op, int __fd, struct epoll_event *__event);

    void s_bind();

    void s_listen();

    void s_epoll_wait();

    void printLocalTime(const time_t &t);

    void updateAliveTime(int sockFd, std::map<int, ClientInfo> &onlineList); //更新存活时间
    void scanAndKickOff(); //长时间不在线的客户端踢下线
    void deleteClientData(int sockFd); //清理客户端数据

};

#endif //INC_2018080601EPOLL_FRAMEWORK_EPOLL_H
