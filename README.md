# PS4 .pkg mounter

Mounts a ps4 .pkg file in read-only mode using FUSE.  No need to extract / copy potentially large data. 

# Build
1. `mkdir build`
2. `cmake -S . -B build -G Ninja`
3. `cmake --build build`

# Run
1. `usage: ./build/ps4_pkgmount --pkgfile=<.pkg file> <mountpoint>`