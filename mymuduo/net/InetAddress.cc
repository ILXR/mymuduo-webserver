#include "mymuduo/net/InetAddress.h"
// #include "Logging.h"
#include "mymuduo/net/Endian.h"
#include "mymuduo/net/SocketsOps.h"

#include <cassert>
#include <netdb.h>
#include <netinet/in.h>

// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };

using namespace mymuduo;
using namespace mymuduo::net;

// static_assert关键字，用来做编译期间的断言，因此叫做静态断言。
// 语法:
//      static_assert(常量表达式，提示字符串)
static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6),
              "InetAddress is same size as sockaddr_in6");
static_assert(offsetof(sockaddr_in, sin_family) == 0, "sin_family offset 0");
static_assert(offsetof(sockaddr_in6, sin6_family) == 0, "sin6_family offset 0");
static_assert(offsetof(sockaddr_in, sin_port) == 2, "sin_port offset 2");
static_assert(offsetof(sockaddr_in6, sin6_port) == 2, "sin6_port offset 2");

InetAddress::InetAddress(uint16_t portArg, bool loopbackOnly, bool ipv6)
{
    static_assert(offsetof(InetAddress, addr6_) == 0, "addr6_ offset 0");
    static_assert(offsetof(InetAddress, addr_) == 0, "addr_ offset 0");
    if (ipv6)
    {
        memZero(&addr6_, sizeof addr6_);
        addr6_.sin6_family = AF_INET6;
        in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
        addr6_.sin6_addr = ip;
        addr6_.sin6_port = sockets::hostToNetwork16(portArg);
    }
    else
    {
        memZero(&addr_, sizeof addr_);
        addr_.sin_family = AF_INET;
        in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
        addr_.sin_addr.s_addr = sockets::hostToNetwork32(ip);
        addr_.sin_port = sockets::hostToNetwork16(portArg);
    }
}

InetAddress::InetAddress(StringArg ip, uint16_t portArg, bool ipv6)
{
    if (ipv6 || strchr(ip.c_str(), ':'))
    {
        memZero(&addr6_, sizeof addr6_);
        sockets::fromIpPort(ip.c_str(), portArg, &addr6_);
    }
    else
    {
        memZero(&addr_, sizeof addr_);
        sockets::fromIpPort(ip.c_str(), portArg, &addr_);
    }
}

string InetAddress::toIpPort() const
{
    char buf[64] = "";
    sockets::toIpPort(buf, sizeof buf, getSockAddr());
    return buf;
}

string InetAddress::toIp() const
{
    char buf[64] = "";
    sockets::toIp(buf, sizeof buf, getSockAddr());
    return buf;
}

uint32_t InetAddress::ipv4NetEndian() const
{
    assert(family() == AF_INET);
    return addr_.sin_addr.s_addr;
}

uint16_t InetAddress::port() const
{
    return sockets::networkToHost16(portNetEndian());
}

static __thread char t_resolveBuffer[64 * 1024];

bool InetAddress::resolve(StringArg hostname, InetAddress *out)
{
    assert(out != NULL);
    struct hostent hent;
    struct hostent *he = NULL;
    int herrno = 0;
    memZero(&hent, sizeof(hent));

    /** 函数  gethostbyname_r
     * 参数说明：name——是网页的host名称，如百度的host名是www.baidu.com
     *                ret——成功的情况下存储结果用。
     *                buf——这是一个临时的缓冲区，用来存储过程中的各种信息，
     *                     一般8192大小就够了，可以申请一个数组char buf[8192]
     *                buflen——是buf缓冲区的大小
     *                result——如果成功，则这个hostent指针指向ret，也就是正确的结果；
     *                        如果失败，则result为NULL
     *                h_errnop——存储错误码
     * 该函数成功返回0，失败返回一个非0的数。
     * 
     * struct hostent {
     *       char *h_name;                      // official name of host
     *       char **h_aliases;                  // alias list
     *       int h_addrtype;                    // host address type——AF_INET || AF_INET6
     *       int h_length;                      // length of address
     *       char **h_addr_list;                // list of addresses
     * };
     * #define h_addr h_addr_list[0]            // for backward compatibility
     */
    int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
    if (ret == 0 && he != NULL)
    {
        assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
        out->addr_.sin_addr = *reinterpret_cast<struct in_addr *>(he->h_addr);
        return true;
    }
    else
    {
        if (ret)
        {
            //   LOG_SYSERR << "InetAddress::resolve";
            perror("InetAddress::resolve\n");
        }
        return false;
    }
}

void InetAddress::setScopeId(uint32_t scope_id)
{
    if (family() == AF_INET6)
    {
        addr6_.sin6_scope_id = scope_id;
    }
}
