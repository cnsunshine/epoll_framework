//
// Created by sunshine on 18-8-6.
//

#include <zconf.h>
#include <fcntl.h>
#include <iostream>
#include <memory.h>
#include <arpa/inet.h>
#include "Epoll.h"

template<typename T>
Epoll<T>::~Epoll() {

}

template<typename T>
Epoll<T>::Epoll(int ServerPort, int EpollSize, int Timeout, int LISTENQ)
        :ServerPort(ServerPort), EpollSize(EpollSize), Timeout(Timeout), LISTENQ(LISTENQ) {
    this->readFunc = NULL;
}

template<typename T>
void Epoll<T>::setNonBlocking(int sock) {
    int flags;
    flags = fcntl(sock, F_GETFL);

    if (flags < 0) {
        std::cout << "[fcntl(sock, F_GETFL)] wrong" << std::endl;
        exit(0);
    }

    flags = flags | O_NONBLOCK;

    if (fcntl(sock, F_SETFL, flags) < 0) {
        std::cout << "[fcntl(sock, F_SETFL, flags)] wrong" << std::endl;
        exit(0);
    }
}

template<typename T>
void Epoll<T>::s_epoll_create(int size) {
    this->epollFd = epoll_create(size);
}

template<typename T>
void Epoll<T>::s_socket() {
    this->listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (this->listenFd == -1) {
        std::cout << "[socket(AF_INET, SOCK_STREAM, 0)] wrong" << std::endl;
        exit(0);
    };
}

template<typename T>
int Epoll<T>::s_epoll_ctl(int __op, int __fd, struct epoll_event *__event) {
    return epoll_ctl(this->epollFd, __op, __fd, __event);
}

template<typename T>
void Epoll<T>::s_bind() {
    struct sockaddr_in EpollAddr;
    bzero(&EpollAddr, sizeof(EpollAddr));
    EpollAddr.sin_family = AF_INET;
    EpollAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    EpollAddr.sin_port = htons(ServerPort);

    bind(this->listenFd, (struct sockaddr *) &EpollAddr, sizeof(EpollAddr));
}

template<typename T>
void Epoll<T>::s_listen() {
    listen(this->listenFd, LISTENQ);

}

template<typename T>
void Epoll<T>::s_epoll_wait() {
    int sockFd;
    ssize_t n;
    struct sockaddr_in clientAddr;
    std::map<int, ClientInfo>::iterator it_onlineList;
    typename std::map<int, T>::iterator it_sendMessagePackageList;


    this->nfds = epoll_wait(this->epollFd, this->events, 20, 0);

    for (int i = 0; i < this->nfds; ++i) {
        if (this->events[i].data.fd == this->listenFd) {
            this->connFd = accept(this->listenFd, (struct sockaddr *) &clientAddr, &(this->clientLen));
            //
            this->onlineList[this->connFd] = {clientAddr, time(0), time(0)};
            if (this->connFd < 0) {
                std::cout << "[Epoll::s_epoll_wait()] accept(...) wrong" << std::endl;
                exit(0);
            }
            this->setNonBlocking(this->connFd);
            std::cout << "[Epoll::s_epoll_wait()] client connect, from " << inet_ntoa(clientAddr.sin_addr)
                      << " on port "
                      << ntohs(clientAddr.sin_port) << std::endl;
            //print login info
            it_onlineList = this->onlineList.find(this->connFd);
            if (it_onlineList != this->onlineList.end()) {
                std::cout << "[Epoll::s_epoll_wait()] login time: ";
                this->printLocalTime(it_onlineList->second.loginTime);
                std::cout << std::endl;
            }

            this->event.data.fd = this->connFd;
            this->event.events = EPOLLIN | EPOLLET | EPOLLOUT;
            this->s_epoll_ctl(EPOLL_CTL_ADD, this->connFd, &(this->event));
        } else if (this->events[i].events & EPOLLIN) {
            if ((sockFd = this->events[i].data.fd) < 0) continue;
            //
            if ((n = read(sockFd, &msgPkgHead, (size_t) sizeof(MessagePackageHeader))) < 0) {
                if (errno == ECONNRESET) {
                    close(sockFd);
                    this->events[i].data.fd = -1;
                } else {
                    std::cout << "[Epoll::s_epoll_wait()] read data error" << std::endl;
                }
            } else if (n == 0) { //client close
                it_onlineList = this->onlineList.find(sockFd);
                if (it_onlineList != onlineList.end()) {
                    std::cout << "[Epoll::s_epoll_wait()] bye, from "
                              << inet_ntoa(it_onlineList->second.sockAddr.sin_addr) << " on port "
                              << ntohs(it_onlineList->second.sockAddr.sin_port) << std::endl;
                    std::cout << "[Epoll::s_epoll_wait()] login time: ";
                    this->printLocalTime(it_onlineList->second.loginTime);
                    std::cout << std::endl;
                }
                close(sockFd);
                this->event.data.fd = sockFd;
                this->s_epoll_ctl(EPOLL_CTL_DEL, sockFd, &(this->event));
                this->deleteClientData(sockFd);
                this->events[i].data.fd = -1;
            } else {
                int msgPkgBSize = this->msgPkgHead.size;
                T msgPkgBody[msgPkgBSize];
                read(sockFd, &msgPkgBody, (size_t) sizeof(T) * msgPkgBSize);
                this->updateAliveTime(sockFd, this->onlineList);
                std::cout << "[Epoll::s_epoll_wait()] get message: pkg size " << this->msgPkgHead.size << std::endl;
                //调用绑定的处理函数
                if(this->readFunc == NULL){
                    std::cout << "not bind solve func" << std::endl;
                    exit(0);
                }
                SocketData<T> response;
                this->readFunc({sockFd, msgPkgBSize, msgPkgBody}, &response);
                this->sendData(response);
                //response.data需要使用栈上数据，如果使用堆上数据，则要改写代码
                //1.add [delete response.data]
                //2.use [智能指针]
            }
            this->event.data.fd = sockFd;
            this->event.events = EPOLLOUT | EPOLLET | EPOLLIN;
            this->s_epoll_ctl(EPOLL_CTL_MOD, sockFd, &(this->event));

        } else if (this->events[i].events & EPOLLOUT) {
            //以下留用，后续可以使用链和环进行数据发送
            sockFd = this->events[i].data.fd;
            it_sendMessagePackageList = this->sendMessagePackageList.find(sockFd);
            if (it_sendMessagePackageList != this->sendMessagePackageList.end()) {
                //todo 发送部分
                this->updateAliveTime(sockFd, this->onlineList);
                this->sendMessagePackageList.erase(it_sendMessagePackageList);
                this->event.data.fd = sockFd;
                this->event.events = EPOLLIN | EPOLLET | EPOLLOUT;
                this->s_epoll_ctl(EPOLL_CTL_MOD, sockFd, &(this->event));
            } else {
                this->event.data.fd = sockFd;
                this->event.events = EPOLLOUT | EPOLLET | EPOLLIN;
                this->s_epoll_ctl(EPOLL_CTL_MOD, sockFd, &(this->event));
            }
        }
    }


}

template<typename T>
void Epoll<T>::init() {
    this->s_epoll_create(EpollSize);
    this->s_socket();
    this->setNonBlocking(this->listenFd);

    this->event.data.fd = this->listenFd;
    this->event.events = EPOLLIN | EPOLLET;

    this->s_epoll_ctl(EPOLL_CTL_ADD, this->listenFd, &(this->event));
    this->s_bind();
    this->s_listen();
    while (true) {
        this->s_epoll_wait();
        this->scanAndKickOff();
    }
}

template<typename T>
void Epoll<T>::printLocalTime(const time_t &t) {
    tm *localTime;
    localTime = localtime(&t);
    std::cout << localTime->tm_year + 1900 << "/" << localTime->tm_mon + 1 << "/" << localTime->tm_mday << " "
              << localTime->tm_hour << ":" << localTime->tm_min << ":" << localTime->tm_sec;
}

template<typename T>
void Epoll<T>::updateAliveTime(int sockFd, std::map<int, ClientInfo> &onlineList) {
    std::map<int, ClientInfo>::iterator it;
    it = onlineList.find(sockFd);
    if (it != onlineList.end()) {
        it->second.lastAliveTime = time(0);
    }
}

template<typename T>
void Epoll<T>::scanAndKickOff() {
    time_t now = time(0);
    std::map<int, ClientInfo>::iterator it;
    it = this->onlineList.begin();
    while (it != this->onlineList.end()) {
        if (now - it->second.lastAliveTime >= Timeout) {
            //todo
            //超时离线
            close(it->first);
            this->event.data.fd = it->first;
            this->s_epoll_ctl(EPOLL_CTL_DEL, it->first, &(this->event));
            //删除onlineList和sendMessagePackageList
            this->deleteClientData(it->first);
            std::cout << "[Epoll::sanAndKickOff()] kick " << it->first << std::endl;
        }
        it++;
    }
}

template<typename T>
void Epoll<T>::deleteClientData(int sockFd) {
    std::map<int, ClientInfo>::iterator it_onlineList;
    it_onlineList = this->onlineList.find(sockFd);
    if (it_onlineList != this->onlineList.end()) {
        this->onlineList.erase(it_onlineList);
    }

    typename std::map<int, T>::iterator it_sendMessagePackageList;
    it_sendMessagePackageList = this->sendMessagePackageList.find(sockFd);
    if (it_sendMessagePackageList != this->sendMessagePackageList.end()) {
        this->sendMessagePackageList.erase(it_sendMessagePackageList);
    }
}

template<typename T>
void Epoll<T>::bindRecvFunc(ReadFunc readFunc) {
    this->readFunc = readFunc;
}

template<typename T>
ssize_t Epoll<T>::sendData(SocketData<T> response) {
    //包头
    MessagePackageHeader messagePackageHeader;
    messagePackageHeader.size = response.size;
    messagePackageHeader.timestamp = time(0);

    void *data = new char[sizeof(MessagePackageHeader) + response.size * sizeof(T)];
    memcpy(data, &messagePackageHeader, sizeof(MessagePackageHeader));
    memcpy(data + sizeof(MessagePackageHeader), response.data, response.size * sizeof(T));
    ssize_t result =  write(response.fd, data, sizeof(MessagePackageHeader) + response.size * sizeof(T));
    delete data;
    return result;
}
