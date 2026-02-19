#    futils - utility library
#    Copyright (c) 2021-2026 on-keyday (https://github.com/on-keyday)
#    Released under the MIT license
#    https://opensource.org/licenses/mit-license.php
FROM ubuntu:devel

RUN mkdir -p /usr/futilsdev/workspace

WORKDIR /usr/futilsdev/workspace

# if you are not in Japan region, you should comment out this line, or change `ftp.jaist.ac.jp/pub/Linux` to your region millor server address.
RUN sed -i 's@archive.ubuntu.com@ftp.jaist.ac.jp/pub/Linux@g' /etc/apt/sources.list

RUN apt-get update && \
    apt-get install -y \
    libunwind-dev\
    python3-lldb-15

RUN apt-get update && \
    apt-get  install -y\
    g++-11

RUN apt-get update && \
    apt-get install -y\
    ninja-build\
    cmake\
    vim\
    unzip\
    curl\
    libpthread-stubs0-dev\
    libssl-dev

RUN apt-get install -y \
    seq-gen\
    net-tools\
    iputils-ping
RUN apt-get update && \
    apt-get install -y\
    zlib1g

RUN apt install -y lsb-release wget software-properties-common gnupg
RUN apt-get -qq update && \
    curl -o /tmp/llvm.sh https://apt.llvm.org/llvm.sh && \
    chmod +x /tmp/llvm.sh && \
    /tmp/llvm.sh 18 all&& \
    for f in /usr/lib/llvm-*/bin/*; do ln -sf "$f" /usr/bin; done && \
    ln -sf clang /usr/bin/cc && \
    ln -sf clang /usr/bin/c89 && \
    ln -sf clang /usr/bin/c99 && \
    ln -sf clang++ /usr/bin/c++ 

RUN apt install -y\
    lldb-18\
    liblldb-18-dev\
    lld\
    zlib1g-dev

RUN apt install -y\
    libzstd-dev

# RUN ln -s /lib/llvm-15/bin/clang++ /bin/clang++
#RUN ln -s /lib/llvm-15/bin/clang /bin/clang
#RUN ln -s /bin/lldb-18 /bin/lldb
RUN ln -s /lib/llvm-18/lib/libc++abi.so.1.0 /lib/llvm-15/lib/libc++abi.so
RUN ln -s /usr/bin/lldb-server-18 /usr/bin/lldb-server-18.0.0
RUN ln -s /bin/g++-11 /bin/g++
RUN unlink /usr/bin/ld
RUN ln -s /usr/bin/ld.lld /usr/bin/ld

RUN curl https://github.com/lldb-tools/lldb-mi/archive/refs/heads/main.zip \
    -o /usr/futilsdev/lldb-mi.zip -L

RUN unzip /usr/futilsdev/lldb-mi.zip -d /usr/futilsdev
RUN rm /usr/futilsdev/lldb-mi.zip

RUN (cd /usr/futilsdev/lldb-mi-main;cmake -G Ninja .)
RUN (cd /usr/futilsdev/lldb-mi-main;cmake --build .)
RUN cp /usr/futilsdev/lldb-mi-main/src/lldb-mi /bin/lldb-mi

ENV LLDB_DEBUGSERVER_PATH=/usr/bin/lldb-server

#COPY ./src/ /usr/futilsdev/workspace/src/
#COPY ./build /usr/futilsdev/workspace/build
#COPY ./CMakeLists.txt  /usr/futilsdev/workspace/CMakeLists.txt
#COPY ./.clang-format /usr/futilsdev/workspace/.clang-format


CMD /bin/bash
