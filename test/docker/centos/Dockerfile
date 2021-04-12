FROM centos:8

RUN yum update -y && yum install -y yum-utils git gcc gcc-c++ clang llvm llvm-devel rpm-build make cmake python3-devel

RUN sed -i "s|^alias|#alias|g" ~/.bashrc && echo "alias ls='ls --color'" >> ~/.bashrc

# using stable branch of AFL++ here
RUN cd / && git clone https://github.com/AFLplusplus/AFLplusplus && \
    cd /AFLplusplus && make source-only && make install && cd /

# dev branch of exeptor
RUN cd / && git clone https://github.com/fuzzah/exeptor -b dev --recursive && \
    cd /exeptor && rm -fr build && mkdir build && cd build && \
    cmake -DBUILD_TESTING=OFF -DYAML_BUILD_SHARED_LIBS=OFF -DYAML_CPP_BUILD_CONTRIB=OFF \
        -DYAML_CPP_BUILD_TOOLS=OFF -DEXEPTOR_TESTS=ON .. && cmake --build . && ./test/exeptor-tests && \
    cd /

# test exeptor on libxml2
# compiler should change to afl-clang-fast and AFL++ should grab AFL_USE_ASAN env variable.
# as we change compiler from gcc to clang the latter may not recognize some options and
# to overcome this we pass CFLAGS='-Qunused-arguments'
RUN cd / && yumdownloader --source libxml2 && rpm -i ./libxml2-*.src.rpm && \
    rm -f ./libxml2-*.src.rpm && yum install -y pkgconfig\(liblzma\) pkgconfig\(zlib\) && \
    cd /root/rpmbuild/SPECS/ && sed -i 's|%configure |%configure --enable-shared=no |g' libxml2.spec && \
    env LD_PRELOAD=/exeptor/build/libexeptor.so EXEPTOR_CONFIG=/exeptor/libexeptor.yaml \
        CFLAGS='-Qunused-arguments' AFL_USE_ASAN=1 rpmbuild -bc /root/rpmbuild/SPECS/libxml2.spec && \
    grep "__asan" /root/rpmbuild/BUILD/libxml2-*/py3/xmllint && grep "__afl" /root/rpmbuild/BUILD/libxml2-*/py3/xmllint && \
    cd / && rm -fr /root/rpmbuild/BUILD/libxml2-*

ENTRYPOINT "/bin/bash"
