/**
 *  Udp.h
 *
 *  Internal class that implements a UDP socket over which messages
 *  can be sent to nameservers. You normally do not have to construct
 *  this class in user space, it is used internally by the Context class.
 *
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2020 - 2021 Copernica BV
 */

/**
 *  Include guard
 */
#pragma once

/**
 *  Dependencies
 */
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <sys/socket.h>
#include "monitor.h"
#include "watchable.h"
#include "inbound.h"
#include <list>
#include <string>

/**
 *  Begin of namespace
 */
namespace DNS {

/**
 *  Forward declarations
 */
class Core;
class Query;
class Loop;
class Ip;
class Response;
class Processor;

/**
 *  Class definition
 */
class Udps : private Watchable
{
public:
    /**
     *  Interface to be implemented by the parent
     */
    class Handler
    {
    public:
        /**
         *  Method that is called when the udp socket has a buffer available
         *  @param  udp     the reporting socket
         */
        virtual void onBuffered(Udps *udp) = 0;
    };

private:
    /**
     *  event loop
     *  @var Loop*
     */
    Loop *_loop;

    /**
     *  The object that is interested in handling responses
     *  @var Handler*
     */
    Handler *_handler;

    /**
     *  Private helper struct to represent a socket
     */
    struct Socket final : private Monitor, private Inbound
    {
        /**
         *  Pointer to a Udp object
         *  @var Udp*
         */
        Udps *parent = nullptr;

        /**
         *  User space identifier of this monitor
         *  @var void *
         */
        void *identifier = nullptr;

        /**
         *  The filedescriptor of the socket
         *  @var int
         */
        int fd = -1;

        /**
         *  All the buffered responses that came in
         *  @var std::list
         */
        std::list<std::pair<Ip,std::basic_string<unsigned char>>> responses;

        /**
         *  Constructor does nothing but store a pointer to a Udp object.
         *  Sockets are opened lazily
         *  @param parent
         */
        Socket(Udps *parent);

        /**
         *  Closes the file descriptor
         */
        ~Socket();

        /**
         *  Check whether this socket has a valid file descriptor
         *  @return bool
         */
        bool valid() const noexcept { return fd > 0; }

        /**
         *  Implements the Monitor interface
         */
        void notify() override;

        /**
         *  Send a query over this socket
         *  @param  ip IP address to send to. The port is always assumed to be 53.
         *  @param  query  The query
         *  @param  buffersize
         *  @return this, or nullptr if something went wrong
         */
        Inbound *send(const Ip &ip, const Query &query, int32_t buffersize);

        /**
         *  Send a query to a certain nameserver
         *  @param  address     target address
         *  @param  size        size of the address
         *  @param  query       query to send
         *  @return bool
         */
        bool send(const struct sockaddr *address, size_t size, const Query &query);

        /**
         *  Open the socket
         *  @param  version     IPv4 or IPv6. You must ensure this stays consistent over multiple requests (@todo)
         *  @param  buffersize  The buffersize
         *  @return bool
         */
        bool open(int version, int32_t buffersize);

        /**
         *  Close the socket
         *  @return bool
         */
        void close() override;

        /**
         *  Invoke callback handlers for buffered raw responses
         *  @param   watcher   to keep track if the parent object remains valid
         *  @param   maxcalls  the max number of callback handlers to invoke
         *  @return  number of callback handlers invoked
         */
        size_t deliver(Watcher *watcher, size_t maxcalls);

        /**
         *  Helper method to set an integer socket option
         *  @param  optname
         *  @param  optval
         */
        int setintopt(int optname, int32_t optval);
    };

    /**
     *  Socket needs access to the loop and the buffersize
     */
    friend class Socket;

    /**
     *  Collection of all sockets
     *  @var std::vector<Socket>
     */
    std::list<Socket> _sockets;

    /**
     *  The next socket to use for sending a new query
     *  @var size_t
     */
    std::list<Socket>::iterator _current;

    /**
     *  Close all sockets
     *  @todo: this method should disappear
     */
    void close();

public:
    /**
     *  Constructor
     *  @param  loop        event loop
     *  @param  handler     object that will receive all incoming responses
     *  @param  socketcount number of UDP sockets to keep open
     *  @throws std::runtime_error
     */
    Udps(Loop *loop, Handler *handler, size_t socketcount = 1);

    /**
     *  No copying
     *  @param  that
     */
    Udps(const Udps &that) = delete;
        
    /**
     *  Destructor
     */
    virtual ~Udps() = default;

    /**
     *  Send a query to the socket
     *  Watch out: you need to be consistent in calling this with either ipv4 or ipv6 addresses
     *  @param  ip          IP address of the target nameserver
     *  @param  query       the query to send
     *  @param  buffersize  strange parameter
     *  @return Inbound     the inbound object over which the message is sent
     *
     *  @todo   buffersize is strange
     */
    Inbound *send(const Ip &ip, const Query &query, int32_t buffersize);

    /**
     *  Deliver messages that have already been received and buffered to their appropriate processor
     *  @param  size_t      max number of calls to userspace
     *  @return size_t      number of processed answers
     */
    size_t deliver(size_t maxcalls);

    /**
     *  Is the socket now readable?
     *  @return bool
     */
    bool readable() const;

    /**
     *  Does the socket have an inbound buffer (meaning: is there a backlog of unprocessed messages?)
     *  @return bool
     */
    bool buffered() const
    {
        // if there's a buffered response in one of the sockets then we consider ourselves buffered
        for (const auto &sock : _sockets) if (!sock.responses.empty()) return true;

        // otherwise we're not buffered
        return false;
    }
};
    
/**
 *  End of namespace
 */
}
