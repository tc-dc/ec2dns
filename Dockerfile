FROM centos:centos7

RUN yum -y groupinstall 'Development Tools' \
 && yum -y install epel-release
RUN yum -y install libcurl-devel openssl-devel ruby-devel clang which \
 && gem install fpm
RUN curl https://cmake.org/files/v3.4/cmake-3.4.1-Linux-x86_64.tar.gz -o /opt/cmake.tgz \
 && tar -xvf /opt/cmake.tgz -C /opt

ENV CC=/usr/bin/clang CXX=/usr/bin/clang++

# Install boost
RUN cd /tmp \
 && curl https://s3.amazonaws.com/tcdc-repo/packages/boost_1_60_0.tar.bz2 -o boost.tar.bz2 \
 && tar -xvf boost.tar.bz2 > /dev/null \
 && cd boost_1_60_0 \
 && ./bootstrap.sh \
 && ./bjam cxxflags=-fPIC cflags=-fPIC -a \
    --with-regex --with-coroutine --with-system --with-thread --with-context \
    -j4 --layout=tagged threading=multi link=static install

# Build the aws-sdk (you'll need mucho-ramo)
RUN cd /tmp \
 && git clone https://github.com/tellapart/aws-sdk-cpp.git \
 && cd /tmp/aws-sdk-cpp \
 && git checkout c33664ec400d477422905320ce26a32b623a5b71
RUN mkdir /tmp/aws-sdk-cpp/build \
 && cd /tmp/aws-sdk-cpp/build \
 && /opt/cmake-3.4.1-Linux-x86_64/bin/cmake \
    -DBUILD_ONLY="aws-cpp-sdk-ec2" \
    -DSTATIC_LINKING=1 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS=-fPIC \
    /tmp/aws-sdk-cpp \
 && make -j4
RUN cd /tmp/aws-sdk-cpp/build && make install

COPY include /tmp/ec2dns/include
COPY src /tmp/ec2dns/src
COPY docker /tmp/ec2dns/docker
COPY test /tmp/ec2dns/test
COPY external /tmp/ec2dns/external
COPY CMakeLists.txt /tmp/ec2dns/

RUN mkdir /tmp/ec2dns/build \
 && cd /tmp/ec2dns/build \
 && /opt/cmake-3.4.1-Linux-x86_64/bin/cmake \
    /tmp/ec2dns \
 && make -j4 ec2dns

# make an rpm
RUN cd /tmp/ec2dns/build \
 && fpm -t rpm \
        -s dir \
        --version 1.8 \
        -n ec2dns \
        --prefix /var/named \
        --after-install /tmp/ec2dns/docker/rpm/scripts/postinstall.sh \
        libec2dns.so

CMD cp /tmp/ec2dns/build/*.rpm /out
