FROM centos:centos7

RUN yum -y groupinstall 'Development Tools'
RUN yum -y install libcurl-devel openssl-devel ruby-devel \
 && gem install fpm
RUN curl https://cmake.org/files/v3.4/cmake-3.4.1-Linux-x86_64.tar.gz -o /opt/cmake.tgz \
 && tar -xvf /opt/cmake.tgz -C /opt

# Build the aws-sdk (you'll need mucho-ramo)
RUN cd /tmp \
 && git clone https://github.com/tellapart/aws-sdk-cpp.git \
 && cd /tmp/aws-sdk-cpp \
 && git checkout f23d7f2e30c3fcd416c1ffdd599307899a03a0fe
RUN mkdir /tmp/aws-sdk-cpp/build \
 && cd /tmp/aws-sdk-cpp/build \
 && /opt/cmake-3.4.1-Linux-x86_64/bin/cmake \
    -DBUILD_ONLY="aws-cpp-sdk-ec2" \
    -DSTATIC_LINKING=1 \
    -DCMAKE_BUILD_TYPE=Release \
    /tmp/aws-sdk-cpp \
 && make -j4
RUN cd /tmp/aws-sdk-cpp/build && make install

ENV EC2_DNS_REV b155e837aa5b1a4d386581561adc7c980ffe972d

# Build ec2dns
RUN cd /tmp \
 && git clone https://github.com/tellapart/ec2dns.git \
 && cd /tmp/ec2dns \
 && git checkout ${EC2_DNS_REV}
RUN mkdir /tmp/ec2dns/build \
 && cd /tmp/ec2dns/build \
 && /opt/cmake-3.4.1-Linux-x86_64/bin/cmake \
    /tmp/ec2dns \
 && make -j4

ADD rpm/scripts/postinstall.sh /tmp/
# make an rpm
RUN cd /tmp/ec2dns/build \
 && fpm -t rpm \
        -s dir \
        --version 1.0 \
        -n ec2dns \
        --prefix /var/named \
        --after-install /tmp/postinstall.sh \
        libec2dns.so

CMD cp /tmp/ec2dns/build/*.rpm /out
