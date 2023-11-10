// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#if CARB_PLATFORM_LINUX

#    include "../logging/Log.h"

#    include <poll.h>
#    include <sys/socket.h>
#    include <sys/un.h>
#    include <sys/wait.h>

#    include <string>
#    include <unistd.h>

#    define CARB_INVALID_SOCKET (-1)

namespace carb
{
namespace extras
{

class DomainSocket
{
public:
    DomainSocket()
    {
    }

    ~DomainSocket()
    {
        closeSocket();
    }

    /**
     * Sends a list of FDs (file descriptors) to another process using Unix domain socket.
     *
     * @param fds       A list of file descriptors to send.
     * @param fdCount   The number of file descriptors to send.
     *
     * @return true if FD was sent successfully.
     */
    bool sendFd(const int* fds, int fdCount)
    {
        size_t fdSize = fdCount * sizeof(int);
        msghdr msg = {};
        cmsghdr* cmsg;
        size_t bufLen = CMSG_SPACE(fdSize);
        char* buf;
        char dup[s_iovLen] = "dummy payload";

        iovec iov = {};
        iov.iov_base = &dup;
        iov.iov_len = strlen(dup);

        buf = reinterpret_cast<char*>(alloca(bufLen));
        memset(buf, 0, bufLen);

        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = buf;
        msg.msg_controllen = bufLen;

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(fdSize);

        memcpy(CMSG_DATA(cmsg), fds, fdSize);

        if (sendmsg(m_socket, &msg, 0) == (-1))
        {
            CARB_LOG_ERROR("sendmsg failed, errno=%d/%s", errno, strerror(errno));
            return false;
        }
        return true;
    }

    /**
     * Receives a list of FDs (file descriptors) from another process using Unix domain socket.
     *
     * @param fds       A list of file descriptors to be received.
     * @param fdCount   The number of file descriptors to be received.
     *
     * @return true if FD was received successfully.
     */
    bool receiveFd(int* fds, int fdCount)
    {
        size_t fdSize = fdCount * sizeof(int);
        msghdr msg = {};
        cmsghdr* cmsg;
        size_t bufLen = CMSG_SPACE(fdSize);
        char* buf;
        char dup[s_iovLen];

        iovec iov = {};
        iov.iov_base = &dup;
        iov.iov_len = sizeof(dup);

        buf = reinterpret_cast<char*>(alloca(bufLen));
        memset(buf, 0, bufLen);

        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = buf;
        msg.msg_controllen = bufLen;

        int result = recvmsg(m_socket, &msg, 0);
        if (result == (-1) || result == 0)
        {
            CARB_LOG_ERROR("recvmsg failed, errno=%d/%s", errno, strerror(errno));
            return false;
        }

        cmsg = CMSG_FIRSTHDR(&msg);
        memcpy(fds, CMSG_DATA(cmsg), fdSize);

        return true;
    }

    /**
     * Starts client to connect to a server.
     *
     * @param socketPath  The name or the path of the socket.
     * @return true if the client was started successfully, and it is connected to a server connection.
     *
     * @remark Example of socketPath: "/tmp/fd-pass.socket"
     */
    bool startClient(const char* socketPath)
    {
        m_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_socket == CARB_INVALID_SOCKET)
        {
            CARB_LOG_ERROR("Failed to create socket, errno=%d/%s", errno, strerror(errno));
            return false;
        }

        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socketPath);

        if (connect(m_socket, (const struct sockaddr*)&addr, sizeof(addr)) == -1)
        {
            CARB_LOG_ERROR("Failed to connect to socket %s, errno=%d/%s", addr.sun_path, errno, strerror(errno));
            return false;
        }
        return true;
    }

    /**
     * Closes the connection and socket.
     *
     * @return nothing
     */
    void closeSocket()
    {
        if (m_socket != CARB_INVALID_SOCKET)
        {
            close(m_socket);
        }
        m_socket = CARB_INVALID_SOCKET;
        m_socketPath.clear();
    }

    /**
     * Starts server to listen for a socket.
     *
     * @param socketPath  The name or the path of the socket.
     * @param backlog     The maximum number of the allowed pending connections.
     *
     * @return true if the server was started successfully, and the client accepted the connection.
     */
    bool startServer(const char* socketPath, int backlog = 10)
    {
        m_socket = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_socket == CARB_INVALID_SOCKET)
        {
            CARB_LOG_ERROR("Failed to create server socket, errno=%d/%s", errno, strerror(errno));
            return false;
        }

        // try closing any previous socket
        if (unlink(socketPath) == -1 && errno != ENOENT)
        {
            CARB_LOG_ERROR("unlinking socket failed, errno=%d/%s", errno, strerror(errno));
            return false;
        }

        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socketPath);

        if (bind(m_socket, (const struct sockaddr*)&addr, sizeof(addr)) == -1)
        {
            CARB_LOG_ERROR("Failed to bind the socket, errno=%d/%s", errno, strerror(errno));
            return false;
        }

        if (listen(m_socket, backlog) == -1)
        {
            CARB_LOG_ERROR("Failed to listen on socket, errno=%d/%s", errno, strerror(errno));
            return false;
        }

        CARB_LOG_INFO("DomainSocket: stream server started at %s", socketPath);
        return true;
    }

    /**
     * Check for incoming connection and accept it if any.
     *
     * @param server      The listening DomainSocket.
     *
     * @return true if connection was accepted.
     */
    bool acceptConnection(DomainSocket& server)
    {
        struct pollfd fds;
        int rc;

        fds.fd = server.m_socket;
        fds.events = POLLIN | POLLPRI;
        fds.revents = 0;
        rc = poll(&fds, 1, 0);

        if (rc == -1)
        {
            CARB_LOG_ERROR("an error on the server socket, errno=%d/%s", errno, strerror(errno));
            return false;
        }

        else if (rc == 0)
        {
            // nothing happened
            return false;
        }

        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        socklen_t addrsize = sizeof(addr);
        m_socket = accept(server.m_socket, (struct sockaddr*)&addr, &addrsize);
        if (m_socket == CARB_INVALID_SOCKET)
        {
            CARB_LOG_ERROR("accepting connection failed, errno=%d/%s", errno, strerror(errno));
            return false;
        }

        // Copy the socket path
        m_socketPath = addr.sun_path;

        CARB_LOG_INFO("DomainSocket: accepted connection on %s", addr.sun_path);
        return true;
    }

private:
    int m_socket = CARB_INVALID_SOCKET; ///< socket descriptor
    std::string m_socketPath; ///< socket path or name
    static const size_t s_iovLen = 256; ///< The length of I/O data in bytes (dummy)
};

} // namespace extras
} // namespace carb

#endif
