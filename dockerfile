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
    libssl-dev

RUN ln -s /lib/llvm-12/bin/clang++ /bin/clang++
RUN ln -s /lib/llvm-12/bin/clang /bin/clang
RUN ln -s /lib/llvm-12/lib/libc++abi.so.1.0 /lib/llvm-12/lib/libc++abi.so

COPY ./src/ /usr/utilsdev/workspace/src/
COPY ./build /usr/utilsdev/workspace/build
COPY ./CMakeLists.txt  /usr/utilsdev/workspace/CMakeLists.txt
COPY ./.clang-format /usr/utilsdev/workspace/.clang-format

CMD /bin/bash
