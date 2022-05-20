# mymuduo

**模仿muduo实现C++网络库**
根据 《Linux多线程服务端编程 使用muduo C++网络库》 by 陈硕 这本书的第8章：*"muduo 网络库的设计与实现"* 所讲述的设计思想，结合对源码的理解，实现的基于Reactor的网络编程库，基本上每个文件都进行了较详细的注释。

在根目录下执行

```shell
./build.sh
```

对项目进行构建。

在根目录下执行

```shell
./install # 执行时需要使用 sodu，键入密码即可
```

将头文件部署到 ```/usr/include/mymuduo```；
将静态库文件部署到 ```/usr/local/lib```。
