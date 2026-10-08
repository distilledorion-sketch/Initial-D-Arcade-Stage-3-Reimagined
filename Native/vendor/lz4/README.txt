LZ4 v1.10.0, unmodified upstream sources.
https://github.com/lz4/lz4/tree/v1.10.0/lib
BSD-2-Clause; see LICENSE. The same license is staged in data/third-party/LZ4.txt.

SHA256 lz4.c: 9396f7de527bc8435de9c7569fb7998e56545a84b4f3c2d808c0235c01774539
SHA256 lz4hc.c: 126cafafdb91767e6e55238298a910903851b35b2cee27ce80ae2280469ee232

Only the ordinary safe block decoder is required at runtime. HC is used by the
build-time packing utility. No external runtime DLL or internet access is needed.
