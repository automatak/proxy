#include "EPollContext.h"

#include "proxy/ErrorCodes.h"

#include <unistd.h>
#include <iostream>

namespace proxy {

    EPollContext::EPollContext(int numFD, int maxEvents, std::error_code &ec) :
            MAX_EVENT(maxEvents),
            events(new epoll_event[maxEvents]),
            epfd(epoll_create(numFD))
    {
        if (epfd < 0)
        {
            ec = std::error_code(errno, std::system_category());
        }
    }

    EPollContext::~EPollContext()
    {
        delete[] events;
    }

    bool EPollContext::Run(std::error_code &ec)
    {
        while (!ec)
        {
            std::cout << "epoll_wait on fd: " << epfd << std::endl;
            int n = epoll_wait(epfd, events, MAX_EVENT, -1);
            std::cout << "Received events: " << n << std::endl;

            if (n < 0)
            {
                ec = std::error_code(errno, std::system_category());
                return false;
            }

            for (int i = 0; i < n; ++i)
            {
                this->ProcessEvent(events[i], ec);
            }
        }

        return true;
    }

    bool EPollContext::ProcessEvent(epoll_event &event, std::error_code &ec)
    {
        SessionContext* context = reinterpret_cast<SessionContext*>(event.data.ptr);

        switch(context->type)
        {
            case(SessionContext::Type::SERVER_FD):
                return this->ProcessServerListenEvent(event, *context->session, ec);
            default:
                ec = Error::UNKNOWN_SESSION_EVENT;
                return false;
        }
    }

    bool EPollContext::ProcessServerListenEvent(const epoll_event& event, Session& session, std::error_code& ec)
    {
        // figure out what kind of event it is on the session
        if (event.events & EPOLLHUP || event.events & EPOLLERR)
        {
            ec = Error::SERVER_LISTEN_ERROR;
            close(session.serverfd);
            return false;
        }

        sockaddr_in clientaddr;
        socklen_t clientlen = sizeof(clientaddr);

        int connfd = accept(session.serverfd, (sockaddr *) &clientaddr, &clientlen);

        if (connfd < 1) {
            ec = std::error_code(errno, std::system_category());
            return false;
        }

        char buffer[INET_ADDRSTRLEN];
        auto address = inet_ntop(AF_INET, &clientaddr.sin_addr, buffer, INET_ADDRSTRLEN);

        std::cout << "Accepted connection from: " << address << std::endl;

        close(connfd);

        return true;
    }

    bool EPollContext::ProcessServerConnEvent(const epoll_event& event, Session& session, std::error_code& ec)
    {
        return false;
    }

    bool EPollContext::ProcessClientConnEvent(const epoll_event& event, Session& session, std::error_code& ec)
    {
        return false;
    }

    bool EPollContext::AddListen(Session &session, std::error_code &ec)
    {
        return this->Modify(EPOLL_CTL_ADD, session.serverfd, EPOLLIN, session.serverListenContext, ec);
    }

    bool EPollContext::Modify(int operation, int fd, uint32_t events, SessionContext& context, std::error_code &ec)
    {
        epoll_event evt;
        evt.events = events;
        evt.data.ptr = &context;

        if (epoll_ctl(epfd, operation, fd, &evt) < 0)
        {
            ec = std::error_code(errno, std::system_category());
            return false;
        }

        std::cout << "Added listen for fid: " << fd << std::endl;

        return true;
    }

}


