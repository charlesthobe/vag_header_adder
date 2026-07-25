Adds vag header to headerless vag files also known as vab files.

You can read more about vag files in [problemkaputt](https://problemkaputt.de/psxspx-cdrom-file-audio-single-samples-vag-sony.htm)

To build, run:
```
git clone https://github.com/charlesthobe/vag_header_adder
cd vag_header_adder
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```
Note: if you are on a debian based distro that doesn't support C++23 such as debian 12 or ubuntu 24.04 you could install LLVM version 23 and `libc++-23-dev` from https://apt.llvm.org/
and compile like so:
```
git clone https://github.com/charlesthobe/vag_header_adder
cd vag_header_adder
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-23 -DCMAKE_CXX_COMPILER=clang++-23 -DCMAKE_CXX_FLAGS="-stdlib=libc++" ..
ninja
```
For usage just invoke the resulting `vag_header_adder` binary with no arguments or with `--help` or `-h` flag.
