# Verus
Verus is an adaptive congestion control protocol that is custom designed for cellular networks.

### Build Instructions:
Required packages: libasio libalglib libboost-system

Steps (tested on Ubuntu 14.04.1):
```sh
$ sudo apt-get install build-essential autoconf libasio-dev libalglib-dev libboost-system-dev
$ ./bootstrap.sh
$ ./configure
$ make
```
