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
For usage just invoke the resulting `vag_header_adder` binary with no arguments or with `--help` or `-h` flag.
