#!/bin/bash
# Run this script in fly project root directory

openssl genrsa 2048 > tests/fly_test.key
openssl req -new -subj '/CN=localhost' -key tests/fly_test.key > tests/fly_test.csr
openssl x509 -req -days 3650 -signkey tests/fly_test.key < tests/fly_test.csr > tests/fly_test.crt
