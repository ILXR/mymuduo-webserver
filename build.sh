#!/bin/sh
# 在执行bash脚本的时候，内核会根据"#！"后的解释器来确定该用哪个程序解释脚本中的内容
# 注意：这一行必须在每个脚本顶端的第一行，如果不是第一行则为脚本注释行

# 显示脚本运行的冗余输出，在set命令之后执行的每一条命令以及加载命令行中的任何参数都会显示出来
# 每一行都会加上加号（+），提示它是跟踪输出的标识
set -x

SOURCE_DIR=`pwd`
# ${file-my.file.txt} ：假如$file 沒有设定，則使用my.file.txt 作传回值。(空值及非空值時不作处理) 
# ${file:-my.file.txt} ：假如$file 沒有設定或為空值，則使用my.file.txt 作傳回值。(非空值時不作处理)
# ${file+my.file.txt} ：假如$file 設為空值或非空值，均使用my.file.txt 作傳回值。(沒設定時不作处理)
# ${file:+my.file.txt} ：若$file 為非空值，則使用my.file.txt 作傳回值。(沒設定及空值時不作处理)
# ${file=my.file.txt} ：若$file 沒設定，則使用my.file.txt 作傳回值，同時將$file 賦值為my.file.txt 。(空值及非空值時不作处理)
# ${file:=my.file.txt} ：若$file 沒設定或為空值，則使用my.file.txt 作傳回值，同時將$file 賦值為my.file.txt 。(非空值時不作处理)
# ${file?my.file.txt} ：若$file 沒設定，則將my.file.txt 輸出至STDERR。(空值及非空值時不作处理)
BUILD_DIR=${BUILD_DIR:-../mymuduo-build}
BUILD_TYPE=${BUILD_TYPE:-release}
INSTALL_DIR=${INSTALL_DIR:-../${BUILD_TYPE}-install-cpp11}
CXX=${CXX:-g++}

# 为某一个文件在另外一个位置建立一个同步的链接
# 当我们需要在不同的目录，用到相同的文件时，我们不需要在每一个需要的目录下都放一个必须相同的文件，
# 我们只要在某个固定的目录，放上该文件，然后在其它的 目录下用ln命令链接（link）它就可以
ln -sf $BUILD_DIR/$BUILD_TYPE-cpp11/compile_commands.json

mkdir -p $BUILD_DIR/$BUILD_TYPE-cpp11 \
  && cd $BUILD_DIR/$BUILD_TYPE-cpp11 \
  && cmake \
           -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
           -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
           -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
           $SOURCE_DIR \
  && make $*
# $*和$@都表示传递给函数或脚本的所有参数，不被双引号“”包含时，都以$1 $2 …$n的形式输出所有参数。
# 但是当它们被双引号" "包含时，就会有区别了：
# "$*"会将所有的参数从整体上看做一份数据，而不是把每个参数都看做一份数据。
# "$@"仍然将每个参数都看作一份数据，彼此之间是独立的。

# Use the following command to run all the unit tests
# at the dir $BUILD_DIR/$BUILD_TYPE :
# CTEST_OUTPUT_ON_FAILURE=TRUE make test

# cd $SOURCE_DIR && doxygen
