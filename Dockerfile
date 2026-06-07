# syntax=docker/dockerfile:1

ARG RUST_VERSION=1.96.0
FROM rust:${RUST_VERSION}-bookworm

ARG RUST_VERSION
ARG NIGHTLY_TOOLCHAIN=nightly-2026-06-06
ARG USER_ID=1000
ARG GROUP_ID=1000

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        cppcheck \
        doxygen \
        git \
        python3 \
    && rm -rf /var/lib/apt/lists/*

RUN rustup toolchain install "${RUST_VERSION}" \
        --profile minimal \
        --component clippy,rustfmt \
    && rustup target add thumbv7em-none-eabi --toolchain "${RUST_VERSION}" \
    && rustup toolchain install "${NIGHTLY_TOOLCHAIN}" \
        --profile minimal \
        --component llvm-tools-preview,miri,rust-src

RUN cargo +"${RUST_VERSION}" install cbindgen --version 0.29.3 --locked \
    && cargo +"${RUST_VERSION}" install cargo-fuzz --version 0.13.1 --locked \
    && cargo +"${RUST_VERSION}" install cargo-llvm-cov --version 0.8.7 --locked \
    && cargo +"${RUST_VERSION}" install rust-code-analysis-cli --version 0.0.25 --locked

RUN if ! getent group "${GROUP_ID}" >/dev/null; then \
        groupadd --gid "${GROUP_ID}" developer; \
    fi \
    && useradd --uid "${USER_ID}" --gid "${GROUP_ID}" \
        --create-home --shell /bin/bash developer \
    && mkdir -p /workspace /home/developer/.cargo/registry /home/developer/.cargo/git \
    && chown -R "${USER_ID}:${GROUP_ID}" /workspace /home/developer

ENV CARGO_HOME=/home/developer/.cargo
ENV PATH=/usr/local/cargo/bin:/home/developer/.cargo/bin:${PATH}

USER developer
WORKDIR /workspace

CMD ["bash"]
