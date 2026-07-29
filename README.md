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
<br>
<br>
<br>
<br>
<br>
Bonus: If you want to make vag files appear as music files in your file browser run these commands while in the project root directory:
```
cp mime/packages/vag.xml ~/.local/share/mime/packages/.
update-mime-database ~/.local/share/mime
```
This is specially useful if you use a music player capable of playing vag files like audacious with [vgmstream](https://github.com/vgmstream/vgmstream) plugin.

And to undo this you can run:
```
rm ~/.local/share/mime/packages/vag.xml 
update-mime-database ~/.local/share/mime
```