[![autotools CI](https://github.com/davea42/libdwarf-code/actions/workflows/ci_linux_autotools.yml/badge.svg)](https://github.com/davea42/libdwarf-code/actions/workflows/ci_linux_autotools.yml)
[![linux_meson CI](https://github.com/davea42/libdwarf-code/actions/workflows/ci_linux_meson.yml/badge.svg)](https://github.com/davea42/libdwarf-code/actions?query=workflows/ci_linux_meson.yml)
[![msys2 CI](https://github.com/davea42/libdwarf-code/actions/workflows/ci_msys2.yml/badge.svg)](https://github.com/davea42/libdwarf-code/actions/workflows/ci_msys2.yml)
[![linux_cmake CI](https://github.com/davea42/libdwarf-code/actions/workflows/ci_linux_cmake.yml/badge.svg)](https://github.com/davea42/libdwarf-code/actions/workflows/ci_linux_cmake.yml)
[![Travis Build Status](https://travis-ci.com/davea42/libdwarf-code.svg?branch=master)](https://travis-ci.com/github/davea42/libdwarf-code)
[![macos_autotools CI](https://github.com/davea42/libdwarf-code/actions/workflows/ci_macos_autotools.yml/badge.svg)](https://github.com/davea42/libdwarf-code/actions/workflows/ci_macos_autotools.yml)

# This is libdwarf README[.md]

Updated 19 April 2022

For release libdwarf-0.4.0 

## REQUIREMENTS from a libdwarf<name>.tar.xz

Mentioning some that might not be automatically
in your base OS release. Restricting attention
here to just building libdwarf and dwarfdump. 

If the objects you work with do not have
compressed-elf-section content zlib/libz
are not required for building/using 
libdwarf/dwarfdump.

    Ubuntu: 
    sudo apt install pkgconf zlib1g zlib1g-dev
    optional add: cmake meson 

    FreeBSD:
    pkg install bash python3 gmake binutils pkgconf lzlib
    optional add: cmake meson

## BUILDING from a libdwarf<name>.tar.xz

This is always recommended as it's not necessary
to have GNU autotools installed.
These examples show doing a build in a directory
different than the source as that is generally
recommended practice. 

### GNU configure build

    rm -rf /tmp/build
    mkdir /tmp/build
    cd /tmp
    tar xf <path to>/libdwarf-0.4.0.tar.xz
    cd  /tmp/build
    /tmp/libdwarf-0.4.0/configure
    make
    make check

### cmake build

README.cmake has details on the available cmake options.

Just like configure, except instead of configure do:

    cmake  /tmp/libdwarf-0.4.0
    make
    ctest -R self

### meson build

    meson 0.45.1  on Ubuntu 18.04 fails.
    meson 0.55.2  on Ubunto 20.04 works.
    meson 0.60.3  on Freebsd 12.2 and Freebsd 13.0 works.


For the simplest example:

    meson /tmp/libdwarf-0.4.0
    ninja -j8

For a faster build with install and sanity tests:

    prefx=/tmp/installtargetmeson
    export CFLAGS="-g -pipe"
    export CXXFLAGS="-g -pipe"
    meson /tmp/libdwarf-0.4.0  \
      --prefix=$prefx \
      --default-library shared
    ninja -j8 install
    ninja test

## BUILDING on linux from a git clone of the source tree with configure

This is not recommended as it requires you have
GNU autotools and pkg-config installed.
Here we assume the source is in  a directory named
/path/to/code

For example, on Ubuntu 20.04
```
    sudo apt-get install autoconf libtool pkg-config
```

Using the source/build directories from above as examples,
do :

    # Standard Linux Build
    cd /path/to/code
    sh autogen.sh
    cd /tmp/build
    /path/to/code/configure
    make
    make check

## BUILDING on MacOS from a git clone of the source tree with configure
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    brew install autoconf automake libtool
    # Then use the  Standard Linux Build lines just above.

## BUILDING from a git clone of the source tree with cmake

It's always recommended to do cmake builds in a clean directory.
See also README.cmake
Instead of configure do:

    cmake  /path/to/code
    make
    ctest -R self

### Options to configure on Linux/Unix 

By default configure compiles and uses libdwarf.a.
With `--enable-shared` appended to the configure step,
both libdwarf.a and libdwarf.so
are built and the runtimes built will reference libdwarf.so.

With `--enable-shared --disable-static`
appended to the configure step,
 libdwarf.so is built and used; libdwarf.a is not built.

Other options of possible interest:

    --enable-wall to turn on compiler diagnostics 
    --enable-dwarfexample to compile the example programs.

    configure -h     shows the options available.  

Sanity checking:
 gcc has some checks that can be done at runtime.
 -fsanitize=undefined is turned on by --enable-sanitize

### Options to configure on Windows

All libdwarf builds are automatically shared object (dll)
builds. No static libdwarf.a is supported.

### Distributing

When ready to create a new source distribution do
a build and then

    make distcheck

# INCOMPATIBILITIES. Changes to interfaces

Comparing libdwarf-0.4.0 to libdwarf-0.3.4
A few  dealloc() functions changed name to have
a consistent pattern for all such.
Access to the DWARF5 .debug_names section
is now fully implemented. 

See the <strong>Recent Changes</strong> section in
libdwarf.pdf (in the release).

[dwhtml]: https://www.prevanders.net/libdwarfdoc/index.html

Or see [the latest online html version] [dwhtml] for the details..

Notice the table of contents at the right edge of the html page.

## Reading DWARF from memory 

If one has DWARF bytes in memory or in a
kind of file system libdwarf cannot understand
one should use 

    dwarf_object_init_b()
    ...call libdwarf functions...
    dwarf_finish()

and create source to provide
functions and data for the three struct
types:

    struct Dwarf_Obj_Access_Interface_a_s
    struct Dwarf_Obj_Access_Methods_a_s
    struct Dwarf_Obj_Access_Section_a_s

These functions and structs now seem complete
(unlike the earlier libdwarf versions), hence
the name and content changes.

For a worked out example of reading DWARF direct from memory
with no file system involved
see

    src/bin/dwarfexample/jitreader.c

David Anderson.
