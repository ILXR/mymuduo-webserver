#ifndef MY_ACCEPTOR_H
#define MY_ACCEPTOR_H

#include "noncopyable.h"
#include "Channel.h"
#include "Socket.h"
#include <boost/function.hpp>

namespace mymuduo
{
    class EventLoop;
    /**
     * 用于accept(2)新TCP连接，并通过回调通知使用者。
     * 它是内部class，供TcpServer使用，生命期由后者控制。
     */
    class Acceptor : noncopyable
    {
    public:
        typedef boost::function<void(int sockfd,
                                     const InetAddress &)>
            NewConnectionCallback;

        Acceptor(EventLoop *loop, const InetAddress &listenAddr);
        ~Acceptor();
        void setNewConnectionCallback(const NewConnectionCallback &cb){
            newConnectionCallback_ = cb;
        };
        bool listening() const { return listening_; }
        void listen();

    private:
        void handleRead(); // readable 回调函数，用于accept连接
        bool listening_;
        EventLoop *loop_;
        Socket acceptSocket_;                         // RAII handle 封装了socket的生命周期
        Channel acceptChannel_;                       // 观察socket上的readable事件
        NewConnectionCallback newConnectionCallback_; // 用户回调函数，accept之后调用
    };
}

#endif // !MY_ACCEPTOR_H