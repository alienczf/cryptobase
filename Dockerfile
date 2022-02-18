FROM rockylinux/rockylinux:8.5

# ------------------------------------------------------------------------------
# Build starts here
# ------------------------------------------------------------------------------
USER root

# add repos
COPY ./*.repo /etc/yum.repos.d/
# hadolint ignore=DL3031,DL3033
RUN dnf -y install epel-release yum-utils sudo
RUN yum-config-manager --enable powertools

RUN dnf -y --setopt=tsflags=nodocs update
RUN dnf -y install \
    google-cloud-sdk \
    openssl-devel \
    libffi-devel \
    sqlite-devel \
    ncurses-devel \
    glibc-devel \
    gdbm-devel \
    tk-devel \
    zlib-devel \
    git \
    git-lfs \
    unzip \
    wget \
    gdb \
    ccache \
    clang \
    clang-tools-extra \
    lldb \
    lld \
    make \
    cmake \
    ninja-build \
    boost-devel \
    python3-devel \
    libxcb-devel \
    procps \
    vim

# python
WORKDIR /tmp
RUN wget https://www.python.org/ftp/python/3.9.6/Python-3.9.6.tgz 
RUN tar xzf Python-3.9.6.tgz
WORKDIR /tmp/Python-3.9.6
RUN ./configure \
    --enable-optimizations \
    --with-ensurepip=install \
    --enable-loadable-sqlite-extensions \
    --prefix=/opt/py39
RUN make install
RUN rm -rf /tmp/Python*


# This user runs sshd, has sudo access for basic bootstrap of containers
ENV UID 7999
RUN groupadd sudo && \
    useradd -rm -d /home/centos -s /bin/bash -G wheel -l -u ${UID} centos && \
    echo "centos:centos" | chpasswd

# setup pyenv (less likely to change stuffs)
WORKDIR /home/centos/cryptobase
COPY requirements.txt ./
RUN /opt/py39/bin/pip3 install --no-cache-dir -r ./requirements.txt

USER ${UID}
WORKDIR /home/centos/cryptobase

ENV PATH="/opt/py39/bin:${PATH}"

# Default to running example
ENTRYPOINT ["bash"]
