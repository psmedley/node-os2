dash ./configure --shared-openssl 2>&1 | tee configure.log
@mkdir out\Release\obj.host\deps
@mkdir out\Release\obj.host\deps\v8
@mkdir out\Release\obj.host\deps\v8\gypfiles
@mkdir out\Release\obj.target\deps\zlib
@mkdir out\Release\obj.target\deps\http_parser
@mkdir out\Release\obj.target\deps\cares
@mkdir out\Release\obj.target\deps\uv
@mkdir out\Release\obj.target\deps\nghttp2
@mkdir out\Release\obj.target\deps\brotli
@mkdir out\Release\obj.target\deps\gtest
rem fix out/makefile
rem replace (for eg u:/dev/node-os2 with /dev/node-os2
rem CXX.target change to g++