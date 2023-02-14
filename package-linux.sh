#!/bin/bash
# Usage: docker build -t packaging-alpine3.17 - < packaging-dockerfile-alpine3.17
#        docker run -v $(pwd):/savvy-src -v $(pwd)/packages:/out packaging-alpine3.17 /savvy-src/package-linux.sh /savvy-src /out


set -euo pipefail

src_dir=$1
out_dir=$2

#linux_version=$(lsb_release -si | tr '[:upper:]' '[:lower:]')-$(lsb_release -sr)


#cget install -DCMAKE_C_FLAGS=-fPIC -DCMAKE_CXX_FLAGS=-fPIC jonathonl/shrinkwrap@v1.0.0-beta --prefix /cget
#cget install -DCMAKE_C_FLAGS=-fPIC -DCMAKE_CXX_FLAGS=-fPIC htslib,https://github.com/samtools/htslib/releases/download/1.6/htslib-1.6.tar.bz2 -DCMAKE_VERBOSE_MAKEFILE=1 --cmake /savvy-src/dep/htslib.cmake --prefix /cget

export CFLAGS="-fPIC"
export CXXFLAGS="-fPIC"

d=`pwd`
cd ${src_dir}
cmake -P cmake/get-dependencies.cmake /cget -DSHRINKWRAP_PREFER_STATIC=ON
cd $d

unset CFLAGS
unset CXXFLAGS


## libsavvy.so old abi
#mkdir build-api-cxx3-abi
#cd build-api-cxx3-abi
#cmake \
#  -DCMAKE_BUILD_TYPE=Release \
#  -DCMAKE_TOOLCHAIN_FILE=/cget/cget/cget.cmake \
#  -DBUILD_SHARED_LIBS=ON \
#  -DCMAKE_CXX_FLAGS="-static-libstdc++" \
#  -DUSE_CXX3_ABI=ON \
#  -DCPACK_SYSTEM_NAME=Linux-CXX3-ABI \
#  -DCPACK_GENERATOR="STGZ;DEB;RPM" \
#  -DCPACK_PACKAGE_CONTACT="csg-devel@umich.edu" \
#  -DCPACK_ARCHIVE_COMPONENT_INSTALL=ON \
#  -DCPACK_DEB_COMPONENT_INSTALL=ON \
#  -DCPACK_RPM_COMPONENT_INSTALL=ON \
#  -DCPACK_COMPONENTS_ALL=api \
#  ${src_dir}
#
##make savvy
#make package
#cp savvy-*.{sh,deb,rpm} ${out_dir}/
#cd ..
#
#
## libsavvy.so new abi
#mkdir build-api-cxx11-abi
#cd build-api-cxx11-abi
#cmake \
#  -DCMAKE_BUILD_TYPE=Release \
#  -DCMAKE_TOOLCHAIN_FILE=/cget/cget/cget.cmake \
#  -DBUILD_SHARED_LIBS=ON \
#  -DCMAKE_CXX_FLAGS="-static-libstdc++" \
#  -DCPACK_GENERATOR="STGZ;DEB;RPM" \
#  -DCPACK_PACKAGE_CONTACT="csg-devel@umich.edu" \
#  -DCPACK_ARCHIVE_COMPONENT_INSTALL=ON \
#  -DCPACK_DEB_COMPONENT_INSTALL=ON \
#  -DCPACK_RPM_COMPONENT_INSTALL=ON \
#  -DCPACK_COMPONENTS_ALL=api \
#  ${src_dir}
#
##make savvy
#make package
#cp savvy-*.{sh,deb,rpm} ${out_dir}/
#cd ..

arc=`uname -m`

# sav cli statically linked
mkdir build-cli
cd build-cli
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/cget \
  -DCMAKE_CXX_FLAGS="-I/cget/include" \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
  -DCMAKE_EXE_LINKER_FLAGS="-static" \
  -DCPACK_GENERATOR="STGZ" \
  -DCPACK_SYSTEM_NAME="Linux-${arc}" \
  -DCPACK_RESOURCE_FILE_LICENSE=${src_dir}/LICENSE \
  -DCPACK_PACKAGE_CONTACT="csg-devel@umich.edu" \
  -DCPACK_ARCHIVE_COMPONENT_INSTALL=ON \
  -DCPACK_DEB_COMPONENT_INSTALL=ON \
  -DCPACK_RPM_COMPONENT_INSTALL=ON \
  -DCPACK_COMPONENTS_ALL=cli \
  ${src_dir}

make sav manuals
make package
cp savvy-*.sh ${out_dir}/
cd ..
