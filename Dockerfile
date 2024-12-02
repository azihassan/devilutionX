FROM azihassan/kallistios:fdffe33635239d46bcccf0d5c4d59bb7d2d91f38

RUN echo "Building unpack_and_minify_mpq..."
RUN git clone https://github.com/diasurgical/devilutionx-mpq-tools/ && \
    cd devilutionx-mpq-tools && \
    cmake -S. -Bbuild-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF && \
    cmake --build build-rel && \
    cmake --install build-rel

RUN echo "Cloning project..."
WORKDIR /opt/toolchains/dc/kos/
RUN git clone -b fix/ISSUE-7-ram-limitations https://github.com/azihassan/devilutionX.git

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
RUN echo "Downloading and unpacking fonts.mpq..."
RUN curl -LO https://github.com/diasurgical/devilutionx-assets/releases/download/v4/fonts.mpq && \
    unpack_and_minify_mpq fonts.mpq && \
    rm fonts.mpq

#spawn version
#RUN echo "Downloading and unpacking spawn.mpq..."
#RUN curl -LO https://raw.githubusercontent.com/d07RiV/diabloweb/3a5a51e84d5dab3cfd4fef661c46977b091aaa9c/spawn.mpq && \
#    unpack_and_minify_mpq spawn.mpq && \
#    rm spawn.mpq

#full version
RUN echo "Copying and unpacking diabdat.mpq..."
COPY DIABDAT.MPQ .
RUN unpack_and_minify_mpq DIABDAT.MPQ

RUN echo "Configuring CMake..."
RUN source /opt/toolchains/dc/kos/environ.sh && \
    #uncomment when using packed save files
    #without this, cmake can't find the kos-ports bzip2 & zlib libraries
    #export CMAKE_PREFIX_PATH=/opt/toolchains/dc/kos-ports/libbz2/inst/:/opt/toolchains/dc/kos-ports/zlib/inst/ && \
    kos-cmake -S. -Bbuild

RUN echo "Compiling..."
RUN source /opt/toolchains/dc/kos/environ.sh && cd build && kos-make

RUN echo "Patching RAM-heavy assets..."
RUN [ -e diabdat ] && \
    cp diabdat/monsters/snake/snakbl.trn diabdat/monsters/snake/snakb.trn && \
    cp blackd.clx diabdat/monsters/black/blackd.clx && \
    cp diablod.clx diabdat/monsters/diablo/diablod.clx && \
    cp diablon.clx diabdat/monsters/diablo/diablon.clx && \
    cp maged.clx diabdat/monsters/mage/maged.clx && \
    patch build/data/txtdata/monsters/monstdat.tsv -l -p0 < monstdat.patch

RUN echo "Generating CDI"
RUN source /opt/toolchains/dc/kos/environ.sh && \
    #mv spawn build/data/spawn && \
    mv fonts/fonts/ build/data/fonts/ && \
    mv diabdat build/data/diabdat && \
    mkdcdisc -e build/devilutionx.elf -o build/devilutionx.cdi --name 'Diablo 1' -d build/data/

ENTRYPOINT ["sh", "-c", "source /opt/toolchains/dc/kos/environ.sh && \"$@\"", "-s"]
