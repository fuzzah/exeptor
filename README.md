# exeptor
libexeptor - helper tool for making fuzzer-instrumented builds. <br>
Your savior library is here to help you fight back against poorly designed build systems that ignore environment variables and instead use hardcoded compilers defined in billions of sed-processed Makefiles, janky SRPM specs, unruly CMake external projects, etc. <br>
How it works: LD_PRELOAD libexeptor to your build-starting command. The library will then intercept all calls to exec functions (execve, execl, posix_spawn, etc) and replace paths according to yaml configuration file. <br>

## Requirements
CMake>=3.4 and compiler with C++11 support to build. <br>
**Linux-based distro**. Other OS are currently not supported. <br>

## Build
Clone this repo and use CMake to build. Don't forget `--recursive` as libexeptor relies on the awesome [yaml-cpp](https://github.com/jbeder/yaml-cpp) library to parse configuration file:
```bash
git clone https://github.com/fuzzah/exeptor --recursive
cd exeptor/
mkdir build && cd build
cmake ..
cmake --build .
```
You'll end up having libexeptor.so and app-proxy in the build directory.

## Run
Set EXEPTOR_CONFIG environment variable with value of **full (absolute) path** to your yaml configuration file (see example below). EXEPTOR_LOG can be used to specify **full path** to log file which will be filled with data about intercepted calls. This file is always appended and is never cleared by libexeptor, so only use it for troubleshooting.
```bash
export EXEPTOR_CONFIG=~/exeptor/libexeptor.yaml
export EXEPTOR_LOG=~/exeptor.log
```
If your build starts with some ELF binary like make or rpmbuild then you may run it like this:
```bash
LD_PRELOAD=~/exeptor/build/libexeptor.so make
```
If your build-starting command is some sort of script then use app-proxy:
```bash
LD_PRELOAD=~/exeptor/build/libexeptor.so ~/exeptor/build/app-proxy ./my_build_starter.sh
```
LD_PRELOAD should also be set to **full path** to libexeptor.so. Full paths are required because most likely your build system will change its current directory many times with libexeptor searching for non-existent configuration files and creating logs everywhere. <br>

As a bonus libexeptor can add desired options (like `-g` and `-fno-omit-frame-pointer`) as well as remove unwanted options (e.g. `-s`, `-Werror`). <br>
Example configuration file: <br>
```yaml
target_groups:
    compilers:
        del-options:
            - -Werror
            - -s
        add-options:
            - -Wno-error
            - -Wno-unknown-pragmas
            - -Wno-unused-value
            - -g
            - -fno-omit-frame-pointer
        replacements:
            gcc: afl-clang-fast
            /usr/bin/gcc: /usr/local/bin/afl-clang-fast
            g++: afl-clang-fast++
            /usr/bin/g++: /usr/local/bin/afl-clang-fast++
            clang: afl-clang-fast
            /usr/bin/clang: /usr/local/bin/afl-clang-fast
            clang++: afl-clang-fast++
            /usr/bin/clang++: /usr/local/bin/afl-clang-fast++
    tools:
        replacements:
            gcc-ranlib: llvm-ranlib
            /usr/bin/gcc-ranlib: /usr/bin/llvm-ranlib
            gcc-ar: llvm-ar
            /usr/bin/gcc-ar: /usr/bin/llvm-ar

```
Here "compilers" and "tools" are just names of groups, they can be anything. In each group there are three possible settings: "replacements" control binary replacements (e.g. search for gcc, replace it with afl-clang-fast), "add-options" and "del-options" change command line arguments (argv) of binaries during replacement. Only matching binaries that need replacement will get their argv changed. <br>
Note that binaries that replace original binaries never get their exec calls intercepted in order to prevent infinite recursion. In this case AFL++ compilers can start gcc/clang without any problems. <br>
<br>
You are advised to create separate config files to perform different builds for different tasks: fuzzing, sanitizing, coverage collection. Fuzzing can also be split by compilers in use: afl-clang-fast++, hfuzz-clang++ and so on.

## FAQ
Q: **Is there really any need for such a tool?** <br>
A: You won't even believe... Not until you see some real build systems used in real production with your own eyes. Some developers may not change their build systems for years (!) because they "just work". When facing with such devs and their systems you SHOULD NOT waste your time finding all the places where compilers and flags are hardcoded, *just use libexeptor instead*. <br>

Q: **My configure command failed: "clang is not able to compile a simple test program". What to do?** <br>
A: Probably you have changed gcc to clang (or something like gcc4->gcc10) and the new compiler doesn't understand some command line arguments passed by **configure**. Try exporting CFLAGS or CXXFLAGS with value **-Qunused-arguments**. <br>

Q: **All the tools in my build system are linked dynamically but exeptor doesn't work! How do I receive help?** <br>
A: Please create an issue describing your build system: OS and tools with versions. If you have a working solution please send a PR (for nonstandard things Dockerfiles are most welcome). <br>

Q: **Can I use it for something other than fuzzing?** <br>
A: Yes, because libexeptor was designed to be 'general purpose': its intended use is to replace original binaries with some similar binaries, e.g. python->python3.8, grep->rg, wget->curl, etc. <br>

Q: **Why call it 'exeptor'?** <br>
A: Well.. exeptor is basically an **exe**c() call interc**eptor**. It could have had some other silly name like execeptor or something completely different, but what would that change? :P <br>

## Troubleshooting
Make sure that all binaries in build system are linked dynamically.<br>
What could help you finding problem: run strace like that (without libexeptor.so in LD_PRELOAD):
```bash
strace -ff -v -s 200 -e process ./build_make_target.sh 2>&1 | egrep "exec\S+\s*\(" | cut -f2 -d\" | sort | uniq | xargs file {} | grep ELF | grep -v "dynamically linked"
```
If you find some ELFs that aren't dynamically linked please try finding or building their dynamically linked versions. Otherwise your resulting builds may end up not being fully instrumented by fuzzer compilers.
Also try setting EXEPTOR_LOG variable while running your build under libexeptor to have a better look at what calls get intercepted.
