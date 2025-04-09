FROM azihassan/kallistios:1b9c7b6bc4858d947a12675c58122951dd42c4db

RUN echo "Patching KOS for streaming a large quantity of files"
RUN source /opt/toolchains/dc/kos/environ.sh && \
    cd /opt/toolchains/dc/kos && \
    sed -i 's/THD_KERNEL_STACK_SIZE (64 \* 1024)/THD_KERNEL_STACK_SIZE (256 \* 1024)/g' /opt/toolchains/dc/kos/kernel/arch/dreamcast/include/arch/arch.h && \
    echo "THD_KERNEL_STACK_SIZE changed to $(cat /opt/toolchains/dc/kos/kernel/arch/dreamcast/include/arch/arch.h | grep THD_KERNEL_STACK_SIZE)" && \
    sed -i 's/THD_STACK_SIZE  32768/THD_STACK_SIZE  (128*1024)/g' /opt/toolchains/dc/kos/kernel/arch/dreamcast/include/arch/arch.h && \
    echo "THD_STACK_SIZE changed to $(cat /opt/toolchains/dc/kos/kernel/arch/dreamcast/include/arch/arch.h | grep THD_STACK_SIZE)" && \
    make clean && \
    make CFLAGS+="-DFS_CD_MAX_FILES=4096 -DFD_SETSIZE=4096 -DMAKEIP_VERSION=2"

RUN echo "Building unpack_and_minify_mpq..."
RUN git clone https://github.com/diasurgical/devilutionx-mpq-tools/ && \
    cd devilutionx-mpq-tools && \
    cmake -S. -Bbuild-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF && \
    cmake --build build-rel && \
    cmake --install build-rel

RUN echo "Cloning project..."
WORKDIR /opt/toolchains/dc/kos/
RUN git clone -b dreamcast https://github.com/azihassan/devilutionX.git

RUN echo "Uninstall kos-ports SDL 1.2..."
RUN source /opt/toolchains/dc/kos/environ.sh && \
    cd /opt/toolchains/dc/kos-ports/SDL && \
    make uninstall || echo 'SDL 1.2 uninstall finished with non zero status, proceding anyway'

RUN echo "Install GPF SDL 1.2..."
RUN git clone -b SDL-dreamhal--GLDC https://github.com/GPF/SDL-1.2 && \
    cd SDL-1.2 && \
    source /opt/toolchains/dc/kos/environ.sh && \
    make -f Makefile.dc && \
    cp /opt/toolchains/dc/kos/addons/lib/dreamcast/libSDL.a /usr/lib/ && \
    cp include/* /usr/include/SDL/

WORKDIR /opt/toolchains/dc/kos/devilutionX
RUN echo "Downloading and unpacking spawn.mpq..."
RUN curl -LO https://raw.githubusercontent.com/d07RiV/diabloweb/3a5a51e84d5dab3cfd4fef661c46977b091aaa9c/spawn.mpq && \
    unpack_and_minify_mpq spawn.mpq && \
    rm spawn.mpq

RUN echo "Downloading and unpacking fonts.mpq..."
RUN curl -LO https://github.com/diasurgical/devilutionx-assets/releases/download/v4/fonts.mpq && \
    unpack_and_minify_mpq fonts.mpq && \
    rm fonts.mpq

#WORKDIR /opt/toolchains/dc/kos/devilutionX
#RUN echo "Copying and unpacking diabdat.mpq..."
#COPY DIABDAT.MPQ .
#RUN unpack_and_minify_mpq DIABDAT.MPQ

RUN echo "Configuring CMake..."
RUN source /opt/toolchains/dc/kos/environ.sh && \
    #uncomment when using packed save files
    #without this, cmake can't find the kos-ports bzip2 & zlib libraries
    #export CMAKE_PREFIX_PATH=/opt/toolchains/dc/kos-ports/libbz2/inst/:/opt/toolchains/dc/kos-ports/zlib/inst/ && \
    kos-cmake -S. -Bbuild

RUN echo "Compiling..."
RUN source /opt/toolchains/dc/kos/environ.sh && cd build && kos-make

RUN echo "Generating CDI"
RUN source /opt/toolchains/dc/kos/environ.sh && \
    mv spawn build/data/spawn && \
    mv fonts/fonts/ build/data/fonts/ && \
    #mv diabdat build/data/diabdat && \
    mkdcdisc -e build/devilutionx.elf -o build/devilutionx.cdi --name 'Diablo 1' -d build/data/

ENTRYPOINT ["sh", "-c", "source /opt/toolchains/dc/kos/environ.sh && \"$@\"", "-s"]
