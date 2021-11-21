# for fly debug on Ubuntu
FROM ubuntu:latest

# system basic
RUN apt update -y
RUN apt install -y build-essential m4 perl autotools-dev autoconf automake libtool-bin autoconf-archive lsof vim
# fly dependent library
RUN apt install -y libssl-dev zlib1g-dev libbrotli-dev
# python install
RUN apt install -y python3-pip

WORKDIR /fly
RUN python3 -m pip install --upgrade pip
# copy fly project directory
COPY . .
RUN pwd
RUN ls -l
RUN python3 -m pip install -r requirements.txt
# build
RUN autoreconf
RUN ./configure
RUN cat config.h
RUN make
RUN make install
RUN python3 setup.py build
# copy extension module(fly module) from build directory to fly directory
RUN cp -f build/lib.linux*/fly/_fly_server.cpython* fly/
# make key file
RUN openssl req \
	-subj '/CN=localhost' \
	-x509 -nodes -days 365 -newkey rsa:2048 \
	-keyout tests/fly_test.key \
	-out tests/fly_test.crt
# python test
RUN python3 -m pytest tests
