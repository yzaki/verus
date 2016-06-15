# Verus
Verus is an adaptive congestion control protocol that is custom designed for cellular networks.
On this branch Verus runs over TCP sockets.

## Build Instructions:

### Linux/Mac
Required packages: libasio libalglib libboost-system

Steps:
```sh
$ sudo apt-get install build-essential autoconf libasio-dev libalglib-dev libboost-system-dev
$ ./bootstrap.sh
$ ./configure
$ make
```

### Android
Verus client only!
```sh
$ $NDK/build/tools/make-standalone-toolchain.sh --toolchain=arm-linux-androideabi-4.9 --platform=android-23 --install-dir=/tmp/toolchain/
$ /tmp/toolchain/bin/arm-linux-androideabi-gcc -o verus_client_arm verus_client.cpp -fPIE -pie -lstdc++
```