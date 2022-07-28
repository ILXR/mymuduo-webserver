#include "mymuduo/http/FileServer.h"

#include "mymuduo/base/Logging.h"
#include "mymuduo/http/HttpContext.h"
#include "mymuduo/http/HttpRequest.h"
#include "mymuduo/http/HttpResponse.h"

#include <sys/stat.h>
#include <cmath>
#include <sstream>
#include <dirent.h>
#include <fcntl.h>
#include <vector>

using namespace mymuduo;
using namespace mymuduo::net;

namespace mymuduo
{
    namespace net
    {
        namespace detail
        {
            string get404Html(string &msg)
            {
                return R"(<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
    <head>
        <meta http-equiv="Content-Type" content="text/html;charset=utf-8">
        <title>Error response</title>
    </head>
    <body>
        <h1>Error response</h1>
        <p>Error code: 404</p>
        <p>Message:)" + msg +
                       R"(</p>
        <p>Error code explanation: HTTPStatus.NOT_FOUND - Nothing matches the given URI.</p>
    </body>
</html>
)";
            }

            void scanDir(const string &path, string &list)
            {
                // <li><a href="branches/">branches/</a></li>
                struct stat buffer;
                struct dirent *dirp; // return value for readdir()
                DIR *dir;            // return value for opendir()

                char fullname[100];     // to store files full name
                size_t n = path.size(); // fullname[n]=='\0'
                strcpy(fullname, path.c_str());
                if (fullname[n - 1] != '/')
                {
                    fullname[n] = '/';
                    fullname[n + 1] = '\0';
                    n++;
                }

                dir = opendir(path.c_str());
                if (!dir)
                    return;
                while ((dirp = readdir(dir)))
                {
                    if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
                        continue;
                    strcpy(&fullname[n], dirp->d_name);
                    stat(fullname, &buffer);

                    char name[600];
                    if (S_ISDIR(buffer.st_mode))
                        snprintf(name, sizeof(name), "<li><a href=\"%s/\">%s/</a></li>", dirp->d_name, dirp->d_name);
                    else
                        snprintf(name, sizeof(name), "<li><a href=\"%s\">%s</a></li>", dirp->d_name, dirp->d_name);
                    list.append(name);
                    list.push_back('\n');
                }
                closedir(dir);
            }
        }
    }
}

pthread_once_t MimeType::once_control = PTHREAD_ONCE_INIT;
std::map<std::string, std::string> MimeType::mime;

void MimeType::init()
{
    mime[".html"] = "text/html;charset=utf-8";
    mime[".avi"] = "video/avi";
    mime[".flv"] = "video/flv";
    mime[".mp4"] = "video/mp4";
    mime[".bmp"] = "image/bmp";
    mime[".doc"] = "application/msword";
    mime[".gif"] = "image/gif";
    mime[".gz"] = "application/x-gzip";
    mime[".htm"] = "text/html;charset=utf-8";
    mime[".ico"] = "image/x-icon";
    mime[".jpg"] = "image/jpeg";
    mime[".png"] = "image/png";
    mime[".txt"] = "text/plain;charset=utf-8";
    mime[".mp3"] = "audio/mp3";
    mime[".cc"] = "text/plain;charset=utf-8";
    mime[".hpp"] = "text/plain;charset=utf-8";
    mime[".h"] = "text/plain;charset=utf-8";
    mime[".cpp"] = "text/plain;charset=utf-8";
    mime[".c"] = "text/plain;charset=utf-8";
    mime[".sh"] = "text/plain;charset=utf-8";
    mime[".py"] = "text/plain;charset=utf-8";
    mime[".md"] = "text/plain;charset=utf-8";
    mime["default"] = "text/html;charset=utf-8";
}

string MimeType::getMime(const std::string &suffix)
{
    pthread_once(&once_control, MimeType::init);
    if (mime.find(suffix) == mime.end())
        return mime["default"];
    else
        return mime[suffix];
}

FileServer::FileServer(const string &path,
                       EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &name,
                       TcpServer::Option option)
    : workPath_(path),
      server_(loop, listenAddr, name, option)
{
    server_.setConnectionCallback(std::bind(&FileServer::onConnection, this, _1));
    server_.setMessageCallback(std::bind(&FileServer::onMessage, this, _1, _2, _3));
}

void FileServer::start()
{
    LOG_WARN << "FileServer[" << server_.name()
             << "] starts listening on " << server_.ipPort();
    server_.start();
}

void FileServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        conn->setContext(HttpContext());
    }
}

void FileServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Timestamp receiveTime)
{
    HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());

    if (!context->parseRequest(buf, receiveTime))
    {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }
    if (context->gotAll())
    {
        onRequest(conn, context->request());
        context->reset();
    }
}

extern char favicon[555];
void FileServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req)
{
    LOG_WARN << "Request : " << req.methodString() << " " << req.path();
    if (req.getVersion() == HttpRequest::kHttp10)
        LOG_WARN << "Http 1.0";
    else
        LOG_WARN << "Http 1.1";
    const string &connection = req.getHeader("Connection");
    bool close = connection == "close" ||
                 (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close);

    // 网站图标
    if (req.path() == "/favicon.ico")
    {
        response.setStatusCode(HttpResponse::k200Ok);
        response.setStatusMessage("OK");
        response.setContentType("image/png");
        response.setBody(string(favicon, sizeof favicon));
    }
    else
        setResponseBody(req, response);

    Buffer buf;
    response.appendToBuffer(&buf);
    conn->send(&buf);
    if (response.needSendFile())
    {
        // 需要发送文件
        int fd = response.getFd();
        size_t needLen = static_cast<size_t>(response.getSendLen());
        conn->sendFile(fd, needLen);
    }

    if (response.closeConnection())
    {
        conn->shutdown();
    }
}

void FileServer::setResponseBody(const HttpRequest &req, HttpResponse &res)
{
    // static const off64_t maxSendLen = 1024 * 1024 * 100;
    string path = workPath_ + req.path();
    struct stat buffer;
    if (stat(path.c_str(), &buffer) == 0)
    {
        if (S_ISDIR(buffer.st_mode))
        { // 目录
            string list, html, show_path = path[0] == '.' ? path.substr(1) : path;
            detail::scanDir(path, list);
            html = R"(<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>Directory listing for )" +
                   show_path +
                   R"(</title>
</head>
<body>
<h1>Directory listing for )" +
                   show_path +
                   R"(</h1>
<hr>
<ul>)" +
                   "\n" + list +
                   R"(</ul>
<hr>
</body>
</html>)";
            res.setStatusCode(HttpResponse::k200Ok);
            res.setContentType("text/html;charset=utf-8");
            res.setBody(html);
        }
        else if (S_ISREG(buffer.st_mode))
        { // 常规文件
            int fd = ::open(path.c_str(), O_RDONLY);
            // off64_t len = lseek(fd, 0, SEEK_END) - lseek(fd, 0, SEEK_SET);
            off64_t len = buffer.st_size;
            res.setFd(fd);

            string suffix;
            size_t pos = req.path().find_last_of('.');
            if (pos != req.path().npos)
                suffix = req.path().substr(pos);
            else
                suffix = "";
            LOG_DEBUG << "File suffix: " << suffix;
            string type = MimeType::getMime(suffix);
            res.setContentType(type);
            res.addHeader("Accept-Ranges", "bytes");

            string range = req.getHeader("Range");
            if (range != "")
            {
                res.setStatusCode(HttpResponse::k206Partitial);
                res.setStatusMessage("Partial Content");

                off64_t beg_num = 0, end_num = 0;
                string range_value = req.getHeader("Range").substr(6);
                pos = range_value.find("-");
                string beg = range_value.substr(0, pos);
                string end = range_value.substr(pos + 1);
                if (beg != "" && end != "")
                {
                    beg_num = stoi(beg);
                    end_num = stoi(end);
                }
                else if (beg != "" && end == "")
                {
                    beg_num = stoi(beg);
                    end_num = len - 1;
                }
                else if (beg == "" && end != "")
                {
                    beg_num = len - stoi(end);
                    end_num = len - 1;
                }

                // 需要读need_len个字节
                // off64_t need_len = std::min(end_num - beg_num + 1, maxSendLen);
                off64_t need_len = end_num - beg_num + 1;
                end_num = beg_num + need_len - 1;
                lseek(fd, beg_num, SEEK_SET);
                res.setSendLen(static_cast<int>(need_len));

                std::ostringstream os_range;
                os_range << "bytes " << beg_num << "-" << end_num << "/" << len;
                res.addHeader("Content-Range", os_range.str());
                res.addHeader("Content-Length", std::to_string(need_len));
                LOG_INFO << os_range.str();
            }
            else
            {
                res.setStatusCode(HttpResponse::k200Ok);
                res.setStatusMessage("OK");
                res.setSendLen(len);
                res.addHeader("Content-Length", std::to_string(len));
            }
        }
        else
        {
            // 非常规文件
            res.setStatusCode(HttpResponse::k404NotFound);
            res.setContentType("text/html;charset=utf-8");
            string msg = "This is not a regular file.";
            res.setBody(detail::get404Html(msg));
        }
    }
    else // 路径不存在，返回 404
    {
        res.setStatusCode(HttpResponse::k404NotFound);
        res.setContentType("text/html;charset=utf-8");
        string msg = "File not found.";
        res.setBody(detail::get404Html(msg));
    }
}

char favicon[555] = {
    '\x89',
    'P',
    'N',
    'G',
    '\xD',
    '\xA',
    '\x1A',
    '\xA',
    '\x0',
    '\x0',
    '\x0',
    '\xD',
    'I',
    'H',
    'D',
    'R',
    '\x0',
    '\x0',
    '\x0',
    '\x10',
    '\x0',
    '\x0',
    '\x0',
    '\x10',
    '\x8',
    '\x6',
    '\x0',
    '\x0',
    '\x0',
    '\x1F',
    '\xF3',
    '\xFF',
    'a',
    '\x0',
    '\x0',
    '\x0',
    '\x19',
    't',
    'E',
    'X',
    't',
    'S',
    'o',
    'f',
    't',
    'w',
    'a',
    'r',
    'e',
    '\x0',
    'A',
    'd',
    'o',
    'b',
    'e',
    '\x20',
    'I',
    'm',
    'a',
    'g',
    'e',
    'R',
    'e',
    'a',
    'd',
    'y',
    'q',
    '\xC9',
    'e',
    '\x3C',
    '\x0',
    '\x0',
    '\x1',
    '\xCD',
    'I',
    'D',
    'A',
    'T',
    'x',
    '\xDA',
    '\x94',
    '\x93',
    '9',
    'H',
    '\x3',
    'A',
    '\x14',
    '\x86',
    '\xFF',
    '\x5D',
    'b',
    '\xA7',
    '\x4',
    'R',
    '\xC4',
    'm',
    '\x22',
    '\x1E',
    '\xA0',
    'F',
    '\x24',
    '\x8',
    '\x16',
    '\x16',
    'v',
    '\xA',
    '6',
    '\xBA',
    'J',
    '\x9A',
    '\x80',
    '\x8',
    'A',
    '\xB4',
    'q',
    '\x85',
    'X',
    '\x89',
    'G',
    '\xB0',
    'I',
    '\xA9',
    'Q',
    '\x24',
    '\xCD',
    '\xA6',
    '\x8',
    '\xA4',
    'H',
    'c',
    '\x91',
    'B',
    '\xB',
    '\xAF',
    'V',
    '\xC1',
    'F',
    '\xB4',
    '\x15',
    '\xCF',
    '\x22',
    'X',
    '\x98',
    '\xB',
    'T',
    'H',
    '\x8A',
    'd',
    '\x93',
    '\x8D',
    '\xFB',
    'F',
    'g',
    '\xC9',
    '\x1A',
    '\x14',
    '\x7D',
    '\xF0',
    'f',
    'v',
    'f',
    '\xDF',
    '\x7C',
    '\xEF',
    '\xE7',
    'g',
    'F',
    '\xA8',
    '\xD5',
    'j',
    'H',
    '\x24',
    '\x12',
    '\x2A',
    '\x0',
    '\x5',
    '\xBF',
    'G',
    '\xD4',
    '\xEF',
    '\xF7',
    '\x2F',
    '6',
    '\xEC',
    '\x12',
    '\x20',
    '\x1E',
    '\x8F',
    '\xD7',
    '\xAA',
    '\xD5',
    '\xEA',
    '\xAF',
    'I',
    '5',
    'F',
    '\xAA',
    'T',
    '\x5F',
    '\x9F',
    '\x22',
    'A',
    '\x2A',
    '\x95',
    '\xA',
    '\x83',
    '\xE5',
    'r',
    '9',
    'd',
    '\xB3',
    'Y',
    '\x96',
    '\x99',
    'L',
    '\x6',
    '\xE9',
    't',
    '\x9A',
    '\x25',
    '\x85',
    '\x2C',
    '\xCB',
    'T',
    '\xA7',
    '\xC4',
    'b',
    '1',
    '\xB5',
    '\x5E',
    '\x0',
    '\x3',
    'h',
    '\x9A',
    '\xC6',
    '\x16',
    '\x82',
    '\x20',
    'X',
    'R',
    '\x14',
    'E',
    '6',
    'S',
    '\x94',
    '\xCB',
    'e',
    'x',
    '\xBD',
    '\x5E',
    '\xAA',
    'U',
    'T',
    '\x23',
    'L',
    '\xC0',
    '\xE0',
    '\xE2',
    '\xC1',
    '\x8F',
    '\x0',
    '\x9E',
    '\xBC',
    '\x9',
    'A',
    '\x7C',
    '\x3E',
    '\x1F',
    '\x83',
    'D',
    '\x22',
    '\x11',
    '\xD5',
    'T',
    '\x40',
    '\x3F',
    '8',
    '\x80',
    'w',
    '\xE5',
    '3',
    '\x7',
    '\xB8',
    '\x5C',
    '\x2E',
    'H',
    '\x92',
    '\x4',
    '\x87',
    '\xC3',
    '\x81',
    '\x40',
    '\x20',
    '\x40',
    'g',
    '\x98',
    '\xE9',
    '6',
    '\x1A',
    '\xA6',
    'g',
    '\x15',
    '\x4',
    '\xE3',
    '\xD7',
    '\xC8',
    '\xBD',
    '\x15',
    '\xE1',
    'i',
    '\xB7',
    'C',
    '\xAB',
    '\xEA',
    'x',
    '\x2F',
    'j',
    'X',
    '\x92',
    '\xBB',
    '\x18',
    '\x20',
    '\x9F',
    '\xCF',
    '3',
    '\xC3',
    '\xB8',
    '\xE9',
    'N',
    '\xA7',
    '\xD3',
    'l',
    'J',
    '\x0',
    'i',
    '6',
    '\x7C',
    '\x8E',
    '\xE1',
    '\xFE',
    'V',
    '\x84',
    '\xE7',
    '\x3C',
    '\x9F',
    'r',
    '\x2B',
    '\x3A',
    'B',
    '\x7B',
    '7',
    'f',
    'w',
    '\xAE',
    '\x8E',
    '\xE',
    '\xF3',
    '\xBD',
    'R',
    '\xA9',
    'd',
    '\x2',
    'B',
    '\xAF',
    '\x85',
    '2',
    'f',
    'F',
    '\xBA',
    '\xC',
    '\xD9',
    '\x9F',
    '\x1D',
    '\x9A',
    'l',
    '\x22',
    '\xE6',
    '\xC7',
    '\x3A',
    '\x2C',
    '\x80',
    '\xEF',
    '\xC1',
    '\x15',
    '\x90',
    '\x7',
    '\x93',
    '\xA2',
    '\x28',
    '\xA0',
    'S',
    'j',
    '\xB1',
    '\xB8',
    '\xDF',
    '\x29',
    '5',
    'C',
    '\xE',
    '\x3F',
    'X',
    '\xFC',
    '\x98',
    '\xDA',
    'y',
    'j',
    'P',
    '\x40',
    '\x0',
    '\x87',
    '\xAE',
    '\x1B',
    '\x17',
    'B',
    '\xB4',
    '\x3A',
    '\x3F',
    '\xBE',
    'y',
    '\xC7',
    '\xA',
    '\x26',
    '\xB6',
    '\xEE',
    '\xD9',
    '\x9A',
    '\x60',
    '\x14',
    '\x93',
    '\xDB',
    '\x8F',
    '\xD',
    '\xA',
    '\x2E',
    '\xE9',
    '\x23',
    '\x95',
    '\x29',
    'X',
    '\x0',
    '\x27',
    '\xEB',
    'n',
    'V',
    'p',
    '\xBC',
    '\xD6',
    '\xCB',
    '\xD6',
    'G',
    '\xAB',
    '\x3D',
    'l',
    '\x7D',
    '\xB8',
    '\xD2',
    '\xDD',
    '\xA0',
    '\x60',
    '\x83',
    '\xBA',
    '\xEF',
    '\x5F',
    '\xA4',
    '\xEA',
    '\xCC',
    '\x2',
    'N',
    '\xAE',
    '\x5E',
    'p',
    '\x1A',
    '\xEC',
    '\xB3',
    '\x40',
    '9',
    '\xAC',
    '\xFE',
    '\xF2',
    '\x91',
    '\x89',
    'g',
    '\x91',
    '\x85',
    '\x21',
    '\xA8',
    '\x87',
    '\xB7',
    'X',
    '\x7E',
    '\x7E',
    '\x85',
    '\xBB',
    '\xCD',
    'N',
    'N',
    'b',
    't',
    '\x40',
    '\xFA',
    '\x93',
    '\x89',
    '\xEC',
    '\x1E',
    '\xEC',
    '\x86',
    '\x2',
    'H',
    '\x26',
    '\x93',
    '\xD0',
    'u',
    '\x1D',
    '\x7F',
    '\x9',
    '2',
    '\x95',
    '\xBF',
    '\x1F',
    '\xDB',
    '\xD7',
    'c',
    '\x8A',
    '\x1A',
    '\xF7',
    '\x5C',
    '\xC1',
    '\xFF',
    '\x22',
    'J',
    '\xC3',
    '\x87',
    '\x0',
    '\x3',
    '\x0',
    'K',
    '\xBB',
    '\xF8',
    '\xD6',
    '\x2A',
    'v',
    '\x98',
    'I',
    '\x0',
    '\x0',
    '\x0',
    '\x0',
    'I',
    'E',
    'N',
    'D',
    '\xAE',
    'B',
    '\x60',
    '\x82',
};