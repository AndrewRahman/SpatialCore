#pragma once

// WR-06: a free loopback UDP port chosen by the operating system, so network tests do
// not share hard-coded port numbers. Two test runs on one machine (ctest -j, two
// checkouts or worktrees) used to collide on 97xx, and because JUCE datagram sockets set
// SO_REUSEADDR a collision does not fail connect(); it splits the packets between the
// two listeners and the counts come out wrong.
//
// juce::DatagramSocket::bindToPort rejects port 0, so the port is found with a plain
// socket: bind to 0.0.0.0:0 (the wildcard JUCE's receiver binds, no SO_REUSEADDR), read back what the OS assigned, close.
// The window between that close and the caller's own bind is a few microseconds and the
// OS is unlikely to hand a just-freed ephemeral port to someone else in it, but nothing
// guarantees that: a concurrent process doing the same lookup could be given the same
// port. The tests accept that small residual risk.

#if defined(_WIN32)
 #include <winsock2.h>
 #include <ws2tcpip.h>
#else
 #include <arpa/inet.h>
 #include <netinet/in.h>
 #include <sys/socket.h>
 #include <unistd.h>
#endif

namespace spatialcore::test
{

/** Returns a port nothing is bound to right now, or 0 if the OS gave none. */
inline int findFreeUdpPort()
{
#if defined(_WIN32)
    // Started once per process: WSAStartup is reference counted, and a call per lookup
    // would never be paired with a WSACleanup.
    static const bool winsockStarted = [] { WSADATA wsa; return WSAStartup (MAKEWORD (2, 2), &wsa) == 0; }();
    if (! winsockStarted)
        return 0;
    const SOCKET s = ::socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
        return 0;
    auto closeSocket = [] (SOCKET h) { ::closesocket (h); };
#else
    const int s = ::socket (AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return 0;
    auto closeSocket = [] (int h) { ::close (h); };
#endif

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = htonl (INADDR_ANY);

    int port = 0;
    if (::bind (s, reinterpret_cast<sockaddr*> (&addr), sizeof (addr)) == 0)
    {
#if defined(_WIN32)
        int len = (int) sizeof (addr);
#else
        socklen_t len = sizeof (addr);
#endif
        if (::getsockname (s, reinterpret_cast<sockaddr*> (&addr), &len) == 0)
            port = (int) ntohs (addr.sin_port);
    }

    closeSocket (s);
    return port;
}

} // namespace spatialcore::test
