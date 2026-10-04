# ==========================================

# Stage 1: Build Environment

# ==========================================

FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install core build dependencies
RUN apt-get update && apt-get install -y \
    curl git build-essential m4 unzip pkg-config \
    libelf-dev sudo autoconf rsync \
    && curl -fsSL https://raw.githubusercontent.com/ocaml/opam/master/shell/install.sh | bash \
    && rm -rf /var/lib/apt/lists/*

# Run opam under a non-root user with passwordless sudo
RUN useradd -m -s /bin/bash builder && echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
USER builder
WORKDIR /home/builder

# Initialize opam and create the OxCaml switch
RUN opam init --bare --disable-sandboxing -y && \
    opam switch create magic-trace 5.2.0+ox \
      --repos "ox=git+https://github.com/oxcaml/opam-repository.git#231c88c2e564fdca40e15e750aacad5fb0887435,default" -y

WORKDIR /home/builder/magic-trace

# --- Layer Caching Step ---
# Copy only the dependency/lock files first so Docker caches the slow opam install step
COPY --chown=builder:builder magic-trace.opam* ./

RUN eval $(opam env --switch=magic-trace) && \
    opam install ./magic-trace.opam --deps-only --locked --with-test -y

# --- NEW: Install LLVM tools and force the system to use the LLVM linker ---
RUN sudo apt-get update && sudo apt-get install -y clang llvm-dev lld libncurses-dev libzstd-dev && \
    sudo ln -sf /usr/bin/ld.lld /usr/bin/ld

# --- Source Build Step ---
# Copy the rest of your local ~/magic-trace checkout
COPY --chown=builder:builder . .

# Build the release binary
RUN eval $(opam env --switch=magic-trace) && \
    dune build --profile release

# ==========================================
# Stage 2: Final Minimal Runtime Image
# ==========================================

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y libelf1 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /home/builder/magic-trace/_build/default/bin/magic_trace_bin.exe /usr/local/bin/magic-trace

ENTRYPOINT ["magic-trace"]
