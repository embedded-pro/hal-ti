FROM gabrielfrasantos/embedded-devcontainer:latest@sha256:259aa0e8fb47d3f216dc244202045bad6046af8408ff0eb60af4b53d9d144849

RUN groupadd --gid 1000 devuser \
    && useradd --uid 1000 --gid devuser --shell /bin/bash --create-home devuser

USER devuser

WORKDIR /home/devuser

HEALTHCHECK NONE
