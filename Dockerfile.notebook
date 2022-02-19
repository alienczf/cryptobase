FROM gcr.io/deeplearning-platform-release/base-cpu:m89
RUN apt update && apt-get install --no-install-recommends -y -q \
    ccache \
    clang \
    clang-format \
    gdb \
    lldb \
    lld \
    cmake \
    ninja-build \
    libboost-all-dev \
    libpython3-dev

COPY . /opt/cryptobase
RUN cd /opt/cryptobase && \
    python3 -m pip install -r requirements.txt && \
    python3 -m pip install -e .