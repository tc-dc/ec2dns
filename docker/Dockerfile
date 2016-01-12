FROM centos:centos7

RUN yum -y groupinstall 'Development Tools' \
 && yum -y install epel-release \
 && yum -y install clang boost
RUN yum -y install libcurl-devel openssl-devel ruby-devel \
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

ENV EC2_DNS_REV 53585fbab4225b0d9f2c108d59c701cd54713f91

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
