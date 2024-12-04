FROM debian:stable-slim

WORKDIR /runner-bootstrap

RUN apt-get update && apt-get -y upgrade && apt-get -y install wget sudo

RUN wget https://github.com/actions/runner/releases/download/v2.321.0/actions-runner-linux-x64-2.321.0.tar.gz && tar xf *.tar.gz && rm *.tar.gz

RUN /runner-bootstrap/bin/installdependencies.sh

RUN useradd runner && adduser runner sudo && echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
RUN mkdir /runner && chown -R runner:runner /runner-bootstrap /runner

COPY entrypoint.sh /runner-bootstrap

ENTRYPOINT [ "/runner-bootstrap/entrypoint.sh" ]