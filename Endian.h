#ifndef MYMUDUO_NET_ENDIAN_H
#define MYMUDUO_NET_ENDIAN_H

#include <stdint.h>
#include <endian.h>

namespace mymuduo
{
    namespace net
    {
        namespace sockets
        {

// the inline assembler code makes type blur, (内联汇编代码使类型模糊)
// so we disable warnings for a while.
#pragma GCC diagnostic push
// 在GCC下，#pragma GCC diagnostic ignored用于表示关闭诊断警告，忽略诊断问题
// 其中-Wformat参数，可以是没有返回值的警告"-Wreturn-type"，
// 函数未使用的警告"-Wunused-function"等各种警告。可以在编译时看到GCC所提示的警告
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"

            // 网络端统一采用大端字节序（高地址存储高位）
            // Intel x86 和 ARM 都是典型的采用小端字节序的CPU
            // C/C++中有如下四个常用的转换函数，这四个函数在小端系统中生效：
            //      htons —— 把unsigned short类型从主机序转成网络字节序
            //      ntohs —— 把unsigned short类型从网络字节序转成主机序
            //      htonl —— 把unsigned long类型从主机序转成网络字节序
            //      ntohl —— 把unsigned long类型从网络字节序转成主机序
            // 大端系统由于和网络字节序相同，所以无需转换
            inline uint64_t hostToNetwork64(uint64_t host64)
            {
                return htobe64(host64);
            }

            inline uint32_t hostToNetwork32(uint32_t host32)
            {
                return htobe32(host32);
            }

            inline uint16_t hostToNetwork16(uint16_t host16)
            {
                return htobe16(host16);
            }

            inline uint64_t networkToHost64(uint64_t net64)
            {
                return be64toh(net64);
            }

            inline uint32_t networkToHost32(uint32_t net32)
            {
                return be32toh(net32);
            }

            inline uint16_t networkToHost16(uint16_t net16)
            {
                return be16toh(net16);
            }

#pragma GCC diagnostic pop

        } // namespace sockets
    }     // namespace net
} // namespace mymuduo

#endif // MYMUDUO_NET_ENDIAN_H
