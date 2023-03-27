FROM debian:latest

# Install required dependencies + nice tools
RUN apt update && apt install -y \
    build-essential \
    libssl-dev \
    libffi-dev \
    libpq-dev \
    git \
    curl \
    wget \
    vim \
    nano \
    pkg-config \
    libnl-3-dev \
    libnl-route-3-dev \
    libnl-genl-3-dev \
    libtool \
    libconfig-dev \
    m4 \
    automake \
    autoconf \
    autoconf-archive \
    strace \
    iputils-ping \
    htop \
    tmux

# Copy all source code
COPY . .

# Setups up the libmoep resources and compiles it
RUN ./setup.sh setupmoep

# Disable ptrace protection
# RUN echo 0 | tee /proc/sys/kernel/yama/ptrace_scope

# Let the container run forever (for dev purposes)
CMD [ "tail", "-f", "/dev/null" ]