FROM centos:centos7

RUN yum -y groupinstall 'Development Tools' \
 && yum -y install epel-release
RUN yum -y install libcurl-devel openssl-devel ruby-devel clang which \
 && gem install fpm
RUN curl https://cmake.org/files/v3.4/cmake-3.4.1-Linux-x86_64.tar.gz -o /opt/cmake.tgz \
 && tar -xvf /opt/cmake.tgz -C /opt

ENV CC=/usr/bin/clang CXX=/usr/bin/clang++

COPY external/boost_1_60_0.tar.bz2 /tmp/boost.tar.bz2
# Install boost
RUN cd /tmp \
 && tar -xvf boost.tar.bz2 > /dev/null \
 && cd boost_1_60_0 \
 && ./bootstrap.sh \
 && ./bjam cxxflags=-fPIC cflags=-fPIC -a \
    --with-regex --with-coroutine --with-system --with-thread --with-context \
    -j4 --layout=tagged threading=multi link=static install

RUN yum install -y libuuid-devel

# Build the aws-sdk (you'll need mucho-ramo)
RUN cd /tmp \
 && git clone https://github.com/aws/aws-sdk-cpp.git \
 && cd /tmp/aws-sdk-cpp \
 && git checkout cfd293eef7ab6698794be652815c8c71bf955c1b
RUN mkdir /tmp/aws-sdk-cpp/build \
 && cd /tmp/aws-sdk-cpp/build \
 && /opt/cmake-3.4.1-Linux-x86_64/bin/cmake \
    -DBUILD_ONLY="ec2;autoscaling" \
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

ENV EC2DNS_VERSION=1.12
# make an rpm
RUN cd /tmp/ec2dns/build \
 && fpm -t rpm \
        -s dir \
        --version ${EC2DNS_VERSION} \
        -n ec2dns \
        --prefix /var/named \
        --after-install /tmp/ec2dns/docker/rpm/scripts/postinstall.sh \
        libec2dns.so

CMD cp /tmp/ec2dns/build/*.rpm /out
