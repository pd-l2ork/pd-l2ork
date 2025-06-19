# Build pd-l2ork using emsdk
FROM emscripten/emsdk:4.0.1 AS builder
WORKDIR /pd-l2ork/
RUN apt-get update && apt-get upgrade -y && apt-get install -y autoconf pkgconf
COPY . .
RUN git submodule update --init --recursive; cd emscripten; make clean; make SHELL="/bin/bash"
RUN perl -pi -e "s/^const BUILD = .+;\$/const BUILD = \"$(date +%Y%m%d)-rev.$(git rev-parse --short HEAD)\";/" /pd-l2ork/emscripten/projects/WebPdL2Ork/index.js
RUN perl -pi -e "s/^const BUILD = .+;\$/const BUILD = \"$(date +%Y%m%d)-rev.$(git rev-parse --short HEAD)\";/" /pd-l2ork/emscripten/projects/WebPdL2Ork/public/js/main.js

# Create container used to run WebPdL2Ork (without the bloat of all the build tools)
FROM node:current-alpine
WORKDIR /WebPdL2Ork/
COPY --from=builder /pd-l2ork/emscripten/projects/WebPdL2Ork .
RUN npm i;
ENTRYPOINT [ "npm", "run", "start" ]
