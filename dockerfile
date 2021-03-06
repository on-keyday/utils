#    utils - utility library
#    Copyright (c) 2021-2022 on-keyday (https://github.com/on-keyday)
#    Released under the MIT license
#    https://opensource.org/licenses/mit-license.php
FROM ubuntu:20.04

RUN mkdir -p /usr/utilsdev/workspace

WORKDIR /usr/utilsdev/workspace

# if you are not in Japan region, you should comment out this line, or change `ftp.jaist.ac.jp/pub/Linux` to your region millor server address.
RUN sed -i 's@archive.ubuntu.com@ftp.jaist.ac.jp/pub/Linux@g' /etc/apt/sources.list

RUN apt-get update && \
    apt-get install -y \
    cmake\
    ninja-build\
    clang-12\
    libc++-12-dev\
    libstdc++-10-dev\
    libpthread-stubs0-dev\
    libssl-dev\
    vim\
    lldb-12\
    liblldb-12-dev\
    curl\
    unzip\
    llvm-12\
    g++-10\
    lld


RUN ln -s /lib/llvm-12/bin/clang++ /bin/clang++
RUN ln -s /lib/llvm-12/bin/clang /bin/clang
RUN ln -s /bin/lldb-12 /bin/lldb
RUN ln -s /lib/llvm-12/lib/libc++abi.so.1.0 /lib/llvm-12/lib/libc++abi.so
RUN ln -s /usr/bin/lldb-server-12 /usr/bin/lldb-server-12.0.0
RUN ln -s /bin/g++-10 /bin/g++

RUN curl https://github.com/lldb-tools/lldb-mi/archive/refs/heads/main.zip \
    -o /usr/utilsdev/lldb-mi.zip -L

RUN unzip /usr/utilsdev/lldb-mi.zip -d /usr/utilsdev
RUN rm /usr/utilsdev/lldb-mi.zip

RUN (cd /usr/utilsdev/lldb-mi-main;cmake -G Ninja .)
RUN (cd /usr/utilsdev/lldb-mi-main;cmake --build .)
RUN cp /usr/utilsdev/lldb-mi-main/src/lldb-mi /bin/lldb-mi


#COPY ./src/ /usr/utilsdev/workspace/src/
#COPY ./build /usr/utilsdev/workspace/build
#COPY ./CMakeLists.txt  /usr/utilsdev/workspace/CMakeLists.txt
#COPY ./.clang-format /usr/utilsdev/workspace/.clang-format


CMD /bin/bash
