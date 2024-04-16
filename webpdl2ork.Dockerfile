# Build pd-l2ork using emsdk
FROM emscripten/emsdk AS builder
WORKDIR /pd-l2ork/
RUN apt-get update && apt-get upgrade -y && apt-get install -y autoconf
COPY . .
RUN cd emscripten; make clean; make

# Create container used to run WebPdL2Ork (without the bloat of all the build tools)
FROM node:current-alpine
WORKDIR /WebPdL2Ork/
COPY --from=builder /pd-l2ork/emscripten/projects/WebPdL2Ork .
RUN npm i; rm public/@pd_extra
COPY --from=builder /pd-l2ork/emscripten/build/pd-l2ork-web/extra public/@pd_extra
ENTRYPOINT [ "npm", "run", "start" ]