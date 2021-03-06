/**
 * @file NetworkConnectionPosix.cpp
 *
 * This module contains the POSIX implementation of the
 * SystemAbstractions::NetworkConnection::Impl class.
 *
 * Copyright (c) 2020 by LiuJ
 */

#include "../NetworkConnectionImpl.hpp"
#include "NetworkConnectionPosix.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace {
    
    static const size_t MAXIMUM_READ_SIZE = 65536;

    static const size_t MAXIMUM_WRITE_SIZE = 65536;

} // namespace

namespace SystemAbstractions {
    
    NetworkConnection::Impl::Impl()
        : platform(new Platform)
        , diagnosticsSender("NetworkConnection")
    {}

    NetworkConnection::Impl::~Impl() noexcept {
        if (platform->processor.joinable()) {
            if (std::this_thread::get_id() == platform->processor.get_id()) {
                platform->processor.detach();
            } else {
                platform->processor.join();
            }
        }
    }

    bool NetworkConnection::Impl::Connect() {
        if (Close(CloseProcedure::ImmediateAndStopProcessor)) {
            brokenDelegate(false);
        }
        struct sockaddr_in socketAddress;
        (void)memset(&socketAddress, 0, sizeof(socketAddress));
        socketAddress.sin_family = AF_INET;
        platform->sock = socket(socketAddress.sin_family, SOCK_STREAM, 0);
        if (platform->sock < 0) {
            diagnosticsSender.SendDiagnosticInformationFormatted(
                SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                "error creating socket: %s",
                strerror(errno)
            );
            return false;
        }
        struct linger linger;
        linger.l_onoff = 1;
        linger.l_linger = 0;
        (void)setsockopt(platform->sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
        if (bind(platform->sock, (struct sockaddr*)&socketAddress, (socklen_t)sizeof(socketAddress)) != 0) {
            diagnosticsSender.SendDiagnosticInformationFormatted(
                SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                "error in bind: %s",
                strerror(errno)
            );
            (void)Close(CloseProcedure::ImmediateDoNotStopProcessor);
            return false;
        }
        (void)memset(&socketAddress, 0, sizeof(socketAddress));
        socketAddress.sin_family = AF_INET;
        socketAddress.sin_addr.s_addr = htonl(peerAddress);
        socketAddress.sin_port = htons(peerPort);
        if (connect(platform->sock, (const sockaddr*)&socketAddress, (socklen_t)sizeof(socketAddress)) != 0) {
            diagnosticsSender.SendDiagnosticInformationFormatted(
                SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                "error in connect: %s",
                strerror(errno)
            );
            (void)Close(CloseProcedure::ImmediateDoNotStopProcessor);
            return false;
        }
        socklen_t socketAddressLength = sizeof(socketAddress);
        if (getsockname(platform->sock, (struct sockaddr*)&socketAddress, &socketAddressLength) == 0) {
            boundAddress = ntohl(socketAddress.sin_addr.s_addr);
            boundPort = ntohs(socketAddress.sin_port);
        }
        int flags = fcntl(platform->sock, F_GETFL, 0);
        flags |= O_NONBLOCK;
        (void)fcntl(platform->sock, F_SETFL, flags);
        return true;
    }

    bool NetworkConnection::Impl::Process() {
        if (platform->sock < 0) {
            diagnosticsSender.SendDiagnosticInformationFormatted(
                SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                "not connected"
            );
            return false;
        }
#ifdef SO_NOSIGPIPE 
        /**
         * 更改套接字，以便它永远不会在write（）上生成SIGPIPE
         */
        int opt = 1;
        if (setsockopt(platform->sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt)) < 0) {
            diagnosticsSender.SendDiagnosticInformationFormatted(
                SystemAbstractions::DiagnosticsSender::Levels::WARNING,
                "error in setsockopt(SO_NOSIGPIPE): %s",
                strerror(errno)
            );
        }
#endif
        if (platform->processor.joinable()) {
            diagnosticsSender.SendDiagnosticInformationString(
                SystemAbstractions::DiagnosticsSender::Levels::WARNING,
                "already processing"
            );
            return true;
        }
        platform->processorStop = false;
        if (!platform->processorStateChangeSignal.Initialize()) {
            diagnosticsSender.SendDiagnosticInformationFormatted(
                SystemAbstractions::DiagnosticsSender::Levels::ERROR,
                "error creating processor state change event: %s",
                platform->processorStateChangeSignal.GetLastError().c_str()
            );
            return false;
        }
        platform->processorStateChangeSignal.Clear();
        const auto self = shared_from_this();
        platform->processor = std::thread([self]{ self->Processor(); });
        return true;
    }

    void NetworkConnection::Impl::Processor() {
        // 拿到 select 设置最大文件句柄数
        const int processorStateChangeSelectHandle = platform->processorStateChangeSignal.GetSelectHandle();
        const int nfds = std::max(processorStateChangeSelectHandle, platform->sock) + 1;
        fd_set readfds, writefds;
        std::vector< uint8_t > buffer;
        std::unique_lock< std::recursive_mutex > processingLock(platform->processingMutex);
        bool wait = true;
        while (
            !platform->processorStop
            && (platform->sock >= 0)
        ) {
            if (wait) {
                FD_ZERO(&readfds);
                FD_ZERO(&writefds);
                FD_SET(platform->sock, &readfds);
                if (platform->outputQueue.GetBytesQueued() > 0) {
                    FD_SET(platform->sock, &writefds);
                }
                FD_SET(processorStateChangeSelectHandle, &readfds);
                processingLock.unlock();
                (void)select(nfds, &readfds, &writefds, NULL, NULL);
                processingLock.lock();
                if (FD_ISSET(processorStateChangeSelectHandle, &readfds) != 0) {
                    platform->processorStateChangeSignal.Clear();
                }
            }
            wait = true;
            if (platform->peerClosed) {
                wait = true;
            } else {
                buffer.resize(MAXIMUM_READ_SIZE);
                const auto amountReceived = recv(platform->sock, (char*)&buffer[0], (int)buffer.size(), MSG_NOSIGNAL);
                if (amountReceived < 0) {
                    if (errno == EWOULDBLOCK) {
                        wait = true;
                    } else {
                        diagnosticsSender.SendDiagnosticInformationString(
                            1,
                            "connection closed abruptly by peer"
                        );
                        if (Close(CloseProcedure::ImmediateDoNotStopProcessor)) {
                            processingLock.unlock();
                            brokenDelegate(false);
                            processingLock.lock();
                        }   
                        break;
                    }
                } else if (amountReceived > 0) {
                    buffer.resize((size_t)amountReceived);
                    wait = false;
                    processingLock.unlock();
                    messageReceivedDelegate(buffer);
                    processingLock.lock();
                } else {
                    diagnosticsSender.SendDiagnosticInformationString(
                        1,
                        "connection closed gracefully by peer"
                    );
                    platform->peerClosed = true;
                    processingLock.unlock();
                    brokenDelegate(true);
                    processingLock.lock();
                }
            }
            if (platform->sock < 0) {
                break;
            }
            const auto outputQueueLength = platform->outputQueue.GetBytesQueued();
            if (outputQueueLength > 0) {
                const auto writeSize = (int)std::min(outputQueueLength, MAXIMUM_WRITE_SIZE);
                buffer = platform->outputQueue.Peek(writeSize);
                const auto amountSent = send(platform->sock, (const char*)&buffer[0], writeSize,  MSG_NOSIGNAL);
                if (amountSent < 0) {
                    if (errno != EWOULDBLOCK) {
                        diagnosticsSender.SendDiagnosticInformationString(
                            1,
                            "connection closed abruptly by peer"
                        );
                        if (Close(CloseProcedure::ImmediateDoNotStopProcessor)) {
                            processingLock.unlock();
                            brokenDelegate(false);
                            processingLock.lock();
                        }
                        break;
                    }
                } else if (amountSent > 0) {
                    (void)platform->outputQueue.Drop(amountSent);
                    if (
                        (amountSent == writeSize)
                        && (platform->outputQueue.GetBytesQueued() > 0) 
                    ) {
                        wait = false;
                    }
                } else {
                    if (Close(CloseProcedure::ImmediateDoNotStopProcessor)) {
                        processingLock.unlock();
                        brokenDelegate(false);
                        processingLock.lock();
                    }
                    break;
                }
            }
            if (
                (platform->outputQueue.GetBytesQueued() == 0)
                && platform->closing
            ) {
                if (!platform->shutdownSent) {
                    shutdown(platform->sock, SHUT_WR);
                    platform->shutdownSent = true;
                }
                if (platform->peerClosed) {
                    CloseImmediately();
                    if (brokenDelegate != nullptr) {
                        processingLock.unlock();
                        brokenDelegate(false);
                        processingLock.lock();
                    }
                }
            }
        }
    }

    bool NetworkConnection::Impl::IsConnected() const {
        return (platform->sock >= 0);
    }

    void NetworkConnection::Impl::SendMessage(const std::vector< uint8_t >& message) {
        std::lock_guard< decltype(platform->processingMutex) > lock(platform->processingMutex);
        
        platform->outputQueue.Enqueue(message);
        platform->processorStateChangeSignal.Set();
    }

    bool NetworkConnection::Impl::Close(CloseProcedure procedure) {
        if (
            (procedure == CloseProcedure::ImmediateDoNotStopProcessor)
            && platform->processor.joinable()
        ) {
            platform->processorStop = true;
            platform->processorStateChangeSignal.Set();
        }
        std::lock_guard< decltype(platform->processingMutex) > lock(platform->processingMutex);
        if (platform->sock >= 0) {
            if (procedure == CloseProcedure::Graceful) {
                platform->closing = true;
                diagnosticsSender.SendDiagnosticInformationString(
                    1,
                    "closing connection"
                );
                platform->processorStateChangeSignal.Set();
            } else {
                CloseImmediately();
                return (brokenDelegate != nullptr);
            }
        }
        return false;
    }

    void NetworkConnection::Impl::CloseImmediately() {
        platform->CloseImmediately();
        diagnosticsSender.SendDiagnosticInformationString(
            1,
            "closed connection"
        );
    }

    uint32_t NetworkConnection::Impl::GetAddressOfHost(const std::string& host) {
        struct addrinfo hints;
        (void)memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        struct addrinfo* rawResults;
        if (getaddrinfo(host.c_str(), NULL, &hints, &rawResults) != 0) {
            return 0;
        }
        std::unique_ptr< struct addrinfo, std::function< void(struct addrinfo*) > > results(
            rawResults,
            [](struct addrinfo* p) {
                freeaddrinfo(p);
            }
        );
        if (results == NULL) {
            return 0;
        } else {
            struct sockaddr_in* ipAddress = (struct sockaddr_in*)results->ai_addr;
            return ntohl(ipAddress->sin_addr.s_addr);
        }
    }

    std::shared_ptr< NetworkConnection > NetworkConnection::Platform::MakeConnectionFromExistingSocket(
        int sock,
        uint32_t boundAddress,
        uint16_t boundPort,
        uint32_t peerAddress,
        uint16_t peerPort
    ) {
        const auto connection = std::make_shared< NetworkConnection >();
        connection->impl_->platform->sock = sock;
        connection->impl_->boundAddress = boundAddress;
        connection->impl_->boundPort = boundPort;
        connection->impl_->peerAddress = peerAddress;
        connection->impl_->peerPort = peerPort;
        return connection;
    }

    void NetworkConnection::Platform::CloseImmediately() {
        (void)close(sock);
        sock = -1;
    }

} // namespace SystemAbstractions

