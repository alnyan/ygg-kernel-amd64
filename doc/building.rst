Building yggdrasil
==================

Building yggdrasil is pretty straightforward and similar to how you
build a Linux distribution. To build a full OS, you'll need:

1. GCC and binutils toolchain.
2. The kernel itself.
3. C library (newlib at the moment of writing).
4. A set of userspace applications (at least some kind of ``/init``).

.. note

    Sadly, I haven't had much time and/or priority to test building
    yggdrasil kernel with clang, but as far as I've tried, there were
    problems causing crashes in early loader stage.

1. Preparation
--------------

Build dependencies:

* GNU binutils and gcc (for host programs), any non-ancient version should
  suffice, I guess.
* git
* make
* autotools (autoconf + automake)

Optional:
* sphinx (for building HTML documentation)

Get the kernel sources:

.. code-block:: shell

    git clone https://git.alnyan.me/alnyan/yggdrasil.git
    cd yggdrasil
    # This is recommended
    git checkout dev

Get the userspace sources:

.. code-block:: shell

    git clone https://git.alnyan.me/alnyan/ygg-userspace.git
    cd ygg-userspace
    # This is recommended
    git checkout dev

Get GNU binutils sources:

.. code-block:: shell

    git clone git://sourceware.org/git/binutils-gdb.git
    cd binutils-gdb
    git checkout binutils-2_33_1

    # OR

    curl -O https://ftp.gnu.org/gnu/binutils/binutils-2.33.1.tar.xz
    tar xf binutils-2.33.1.tar.xz

Get yggdrasil-newlib sources:

.. code-block:: shell

    git clone https://git.alnyan.me/alnyan/newlib-yggdrasil.git
    cd newlib-yggdrasil
    # Make sure to check out "yggdrasil" branch
    git checkout yggdrasil

Additionally, newlib uses legacy autotools features, so you'll need to
install autoconf v2.65 and automake v1.11 into some location, say, ``<old autotools>``:

.. code-block:: shell

    curl -O https://ftp.gnu.org/gnu/automake/automake-1.11.tar.gz
    curl -O https://ftp.gnu.org/gnu/autoconf/autoconf-2.65.tar.gz
    tar xf automake-1.11.tar.gz
    tar xf autoconf-2.65.tar.gz
    mkdir automake-build autoconf-build

    # Build old automake
    cd automake-build
    ../automake-1.11/configure --prefix=<old autotools>
    make && make install

    # Build old autotools
    cd autoconf-build
    ../autoconf-2.64/configure --prefix=<old autotools>
    make && make install

2. The toolchain
----------------

First, you'll need to set up a proper cross-compiler environment for
yggdrasil. While it's possible to just follow instructions on
`osdev wiki page <https://wiki.osdev.org/GCC_Cross-Compiler>`_ to build bare
kernel, building a full yggdrasil OS-specific toolchain is required for
compiling userspace applications making use of libc (newlib at the moment of
writing) and kernel headers.

Current patches are based on:

* binutils 2.33.1
* gcc 9.3.0

After getting all the sources, you should apply yggdrasil patches to binutils
and GCC:

.. code-block:: shell

    cd <binutils sources>
    git apply <yggdrasil sources>/etc/patches/binutils/0001-*.patch
    cd <gcc sources>
    git apply <yggdrasil sources>/etc/patches/gcc/0001-*.patch
    # This is required!
    cd <gcc directory>/libstdc++-v3 && autoconf

Configure, build and install binutils:

.. code-block:: shell

    mkdir binutils-build
    cd binutils-build
    <binutils sources>/configure \
        --target=x86_64-elf-yggdrasil \
        --disable-nls \
        --with-sysroot \
        --prefix=<toolchain prefix>
    make && make install

Full GCC installation cannot be performed right now because there's no libc
in place and kernel headers were not yet installed. Just run similar commands to
install "stage 1" GCC:

.. code-block:: shell

    mkdir gcc-build
    cd gcc-build
    <gcc sources>/configure \
        --target=x86_64-elf-yggdrasil \
        --disable-nls \
        --without-headers \
        --enable-languages=c,c++
        --prefix=<toolchain prefix>
    make all-gcc && make all-target-libgcc && \
        make install-gcc && make install-target-libgcc

I'd suggest a coffebreak now, these commands are going to take much time.

**Optionally**, you may also want to build a 32-bit (i686-elf) cross-compiler
for building the bootloader stub in the kernel. This doesn't require any patching,
just run the following command to build it from the same sources you've cloned
for building binutils and GCC in previous stages:

.. code-block:: shell

    # In your "main" directory
    mkdir binutils-build-32
    cd binutils-build-32
    <binutils sources>/configure \
        --target=i686-elf \
        --disable-nls \
        --with-sysroot \
        --prefix=<32-bit toolchain prefix>
    make && make install

    # In some "main" directory again
    mkdir gcc-build-32
    cd gcc-build-32
    <gcc sources>/configure \
        --target=i686-elf \
        --disable-nls \
        --without-headers \
        --enable-languages=c,c++ \
        --prefix=<32-bit toolchain prefix>
    make all-gcc && make all-target-libgcc && \
        make install-gcc && make install-target-libgcc

Again, this is going to take a while.

.. note

    While it's not necessary to build this 32-bit toolchain, it's considered
    to be a good practive when cross-compiling for bare-metal environment.
    See `osdev wiki page <https://wiki.osdev.org/GCC_Cross-Compiler>`_.

3. Building the kernel
----------------------

I recommend creating some kind of "env" file for easier environment
setup when working with the toolchain:

.. code-block:: shell

    export ARCH=amd64
    export KERNEL_DIR=<yggdrasil sources>
    export PATH="<toolchain prefix>/bin:$PATH"
    export INSTALL_HDR=<toolchain prefix>/x86_64-elf-yggdrasil/include

    # Uncomment the "####" line to use your system's compiler (assuming x86/x86-64)
    # in case you've decided not to build i686-elf- toolchain for cross-compiling
    #### export CC86="gcc -m32"

The kernel is then built using ``make`` command, but first you'll need to provide a
config file for it (just copy ``defconfig``):

.. code-block:: shell

    cd <kernel sources>
    cp defconfig config
    make

Now that the kernel itself is built, you can try and test it. For that you'll need
to make an ISO image with grub:

.. code-block:: shell

    mkdir -p image/boot/grub
    cat >image/boot/grub/grub.cfg <<EOF
    menuentry "yggdrasil" {
        multiboot /boot/loader
        module /boot/kernel kernel
    }
    EOF
    cp <kernel sources>/build/loader.elf image/boot/loader
    cp <kernel sources>/build/kernel.elf image/boot/kernel
    grub-mkrescue -o image.iso image

Then, you can boot the image using qemu:

.. code-block:: shell

    qemu-system-x86_64 -cdrom image.iso -serial stdio

Don't panic when you see "fatal error", that happens because we haven't yet
built any initial ramdisk for kernel to run /init from:

.. code-block:: shell

    # This is absolutely okay:
    [00000002 user_init_func] Starting user init
    [00000002 user_init_func] ram0: No such file or directory
    [00000003 panicf] --- Panic ---
    [user_init_func] Fail
    [00000003 panicf] --- Panic ---

Once the kernel is built and verified to boot, you should install headers into
your toolchain prefix so libc can be built:

.. code-block:: shell

    cd <kernel sources>
    ./install-hdr.sh

4. Building newlib
------------------

Before starting with building newlib, I recommend making an "environment" file for
further use here, too:

.. code-block:: shell

    # For x86_64-elf-yggdrasil- toolchain
    export PATH="<toolchain prefix>/bin:$PATH"
    # For old autotools
    export PATH="<old autotools>/bin:$PATH"

A separate environment file is recommended, because it has older autotools which
may conflict with autotools used for building other parts (non-newlib).

After setting up the environment, run:

.. code-block:: shell

    mkdir newlib-build
    cd newlib-build
    <newlib sources>/configure \
        --prefix=<toolchain prefix> \
        --target=x86_64-elf-yggdrasil
    make && make install

Once this is completed, you're ready to build userspace binaries for yggdrasil.

5. Building the userspace
-------------------------

The userspace is built by simply running ``make`` in "userspace" source directory:

.. code-block:: shell

    cd <userspace sources>
    make

The result of these commands is ``<userspace sources>/build/initrd.img`` file, which is
used as initial ramdisk for the operating system.

6. Making the final image and testing
-------------------------------------

The final OS image is built by combining all the userspace+kernel parts and is similar
to how the bare kernel was tested first:

.. code-block:: shell

    mkdir -p image/boot/grub
    cat >image/boot/grub/grub.cfg <<EOF
    menuentry "yggdrasil" {
        multiboot /boot/loader
        module /boot/kernel kernel
        module /boot/initrd.img initrd
    }
    EOF
    cp <kernel sources>/build/loader.elf image/boot/loader
    cp <kernel sources>/build/kernel.elf image/boot/kernel
    cp <userspace sources>/build/initrd.img image/boot/initrd.img
    grub-mkrescue -o image.iso image

Finally, the resulting image can be booted using qemu (or you can try running it
on your PC, I'd appreciate the feedback from running it on actual hardware):

.. code-block:: shell

    qemu-system-x86_64 -cdrom image.iso -serial stdio

Once the system boots up, you should see the login prompt, where you can type the
combination of ``root`` and ``toor`` to enter the shell as root.

*Congratulations! You've successfully completed the quest of manually building
yggdrasil OS.*
