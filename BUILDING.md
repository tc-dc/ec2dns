Required libraries (OSX), all can be brew installed:
- glog
- gflags
- curl
- openssl
- boost

Build the google-api-cpp-client and copy include and lib to /usr/local/(include/lib)

For building the RPM in docker, you'll also need boost 1.60.0, you can download it here:
  https://sourceforge.net/projects/boost/files/boost/1.60.0/boost_1_60_0.tar.bz2/download
Put it in external/