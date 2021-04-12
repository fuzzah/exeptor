# Docker tests for exeptor

Dockerfiles here create some build configurations **and test** libexeptor.so on them. This mainly exist to locally test things like rpmbuild which is usually only found on RPM-based distributions. <br>

Example of image building (CentOS):
```
git clone https://github.com/fuzzah/exeptor --recursive
cd exeptor/test/docker/centos
docker build -t exeptor-centos .
```
If it builds, exeptor should work on a 'real' system with similar build tools. As a bonus now you have the image with AFL++ and exeptor in OS of choice.<br>
To use it just start a container and optionally pass your source code directory (with **-v**):
```
docker run --rm -it -v /some/host/src:/src exeptor-centos
```

## Dockerfiles
This section describes existing Dockerfiles and what they test. <br>

### CentOS - rpmbuild
On **CentOS 8** we build libxml2 from Source RPM (SRPM) package using its spec file. <br>
We still have to mess with the spec file using sed to pass `--enable-shared=no` to **configure** command to actually build xmllint as ELF. <br>
We also pass `CFLAGS='-Qunused-arguments'` because clang doesn't understand some options defined in libxml2.spec for gcc.

