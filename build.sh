#!/bin/bash
set -ev

here=$(dirname "$(readlink -f "$BASH_SOURCE")")

sudo apt-get update -qq
sudo apt-get -y install \
  autoconf \
  automake \
  build-essential \
  cmake \
  git-core \
  libass-dev \
  libfreetype6-dev \
  libgnutls28-dev \
  libmp3lame-dev \
  libsdl2-dev \
  libtool \
  libva-dev \
  libvdpau-dev \
  libvorbis-dev \
  libxcb1-dev \
  libxcb-shm0-dev \
  libxcb-xfixes0-dev \
  meson \
  ninja-build \
  pkg-config \
  texinfo \
  wget \
  yasm \
  zlib1g-dev \
  libdav1d-dev \
  libopus-dev \
  libvpx-dev \
  libx265-dev \
  libnuma-dev \
  libx264-dev \
  libunistring-dev

cd "$HOME/repo/contrib"
git clone git@github.com:stergiotis/FFmpeg.git || true
cd FFmpeg/
dir="$HOME/repo/contrib/FFmpeg"
export PKG_CONFIG_PATH="$dir/lib/pkgconfig"
./configure \
  --extra-cflags="" \
  --extra-cxxflags="" \
  --extra-ldflags="" \
  --prefix="$dir" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$dir/include" \
  --extra-ldflags="-L$dir/lib" \
  --extra-libs="-lpthread -lm" \
  --ld="g++" \
  --disable-ffmpeg \
  --disable-ffprobe \
  --disable-doc \
  --enable-gpl \
  --enable-gnutls \
  --enable-nonfree \
  --enable-libass \
  --enable-libfreetype \
  --enable-libmp3lame \
  --enable-libopus \
  --enable-libdav1d \
  --enable-libvorbis \
  --enable-libvpx \
  --enable-libx264 \
  --enable-libx265
make -j 3
#make install
#hash -r
