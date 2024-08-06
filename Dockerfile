FROM alpine-kallistios:no-gdb

RUN echo "Building unpack_and_minify_mpq..."
RUN git clone https://github.com/diasurgical/devilutionx-mpq-tools/ && \
    cd devilutionx-mpq-tools && \
    cmake -S. -Bbuild-rel -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF && \
    cmake --build build-rel && \
    cmake --install build-rel

RUN echo "Cloning project..."
WORKDIR /opt/toolchains/dc/kos/
RUN git clone -b ci https://github.com/azihassan/devilutionX.git

WORKDIR /opt/toolchains/dc/kos/devilutionX
RUN echo "Injecting spawn.mpq..."
RUN curl -LO https://raw.githubusercontent.com/d07RiV/diabloweb/3a5a51e84d5dab3cfd4fef661c46977b091aaa9c/spawn.mpq && \
    unpack_and_minify_mpq spawn.mpq --output-dir data && \
    rm spawn.mpq

RUN echo "Configuring CMake..."
RUN source /opt/toolchains/dc/kos/environ.sh && \
    export CMAKE_PREFIX_PATH=/opt/toolchains/dc/kos-ports/libbz2/inst/:/opt/toolchains/dc/kos-ports/zlib/inst/ && \
    kos-cmake -S. -Bbuild
RUN patch build/_deps/libfmt-src/include/fmt/format.h -l -p0 < libfmt-long-double.patch

RUN echo "Compiling..."
RUN source /opt/toolchains/dc/kos/environ.sh && cd build && kos-make

RUN cp -R data/spawn build/data/spawn

RUN echo "Generate CDI"
RUN source /opt/toolchains/dc/kos/environ.sh && \
    cp -R data/spawn build/data/spawn && \
    mkdcdisc -e build/devilutionx.elf -o build/devilutionx.cdi --name 'Diablo 1' -d build/data/

ENTRYPOINT ["sh", "-c", "source /opt/toolchains/dc/kos/environ.sh && \"$@\"", "-s"]
