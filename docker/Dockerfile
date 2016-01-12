FROM centos:centos7

RUN yum -y groupinstall 'Development Tools' \
 && yum -y install epel-release
RUN yum -y install libcurl-devel openssl-devel ruby-devel clang which \
 && gem install fpm
RUN curl https://cmake.org/files/v3.4/cmake-3.4.1-Linux-x86_64.tar.gz -o /opt/cmake.tgz \
 && tar -xvf /opt/cmake.tgz -C /opt

ENV CC=/usr/bin/clang CXX=/usr/bin/clang++

# Build the aws-sdk (you'll need mucho-ramo)
RUN cd /tmp \
 && git clone https://github.com/tellapart/aws-sdk-cpp.git \
 && cd /tmp/aws-sdk-cpp \
 && git checkout 3e1b57e75df2bfb68997f3454ce4ae1edda6b856
RUN mkdir /tmp/aws-sdk-cpp/build \
 && cd /tmp/aws-sdk-cpp/build \
 && /opt/cmake-3.4.1-Linux-x86_64/bin/cmake \
    -DBUILD_ONLY="aws-cpp-sdk-ec2" \
    -DSTATIC_LINKING=1 \
    -DCMAKE_BUILD_TYPE=Release \
    /tmp/aws-sdk-cpp \
 && make -j4
RUN cd /tmp/aws-sdk-cpp/build && make install

ENV EC2_DNS_REV c822ba7e888db56946e2075aa8e373c62cbaf013

RUN cd /tmp \
 && curl http://iweb.dl.sourceforge.net/project/boost/boost/1.60.0/boost_1_60_0.tar.bz2 -o boost.tar.bz2 \
 && tar -xvf boost.tar.bz2 > /dev/null \
 && cd boost_1_60_0 \
 && ./bootstrap.sh \
 && ./bjam cxxflags=-fPIC cflags=-fPIC -a --with-regex -j4 --layout=tagged threading=multi link=static install

# Build ec2dns
RUN cd /tmp \
 && git clone https://github.com/tellapart/ec2dns.git \
 && cd /tmp/ec2dns \
 && git checkout ${EC2_DNS_REV}
RUN mkdir /tmp/ec2dns/build \
 && cd /tmp/ec2dns/build \
 && /opt/cmake-3.4.1-Linux-x86_64/bin/cmake \
    /tmp/ec2dns \
 && make -j4 ec2dns

ADD rpm/scripts/postinstall.sh /tmp/
# make an rpm
RUN cd /tmp/ec2dns/build \
 && fpm -t rpm \
        -s dir \
        --version 1.1 \
        -n ec2dns \
        --prefix /var/named \
        --after-install /tmp/postinstall.sh \
        libec2dns.so

CMD cp /tmp/ec2dns/build/*.rpm /out
