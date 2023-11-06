#!/bin/bash

pushd build && cmake .. && make \
    && cp ../config_reference.json ../config_test.json \
    && cp ../schema_reference.json ../schema_test.json \
    && ./FlexConfig ../config_test.json ../schema_test.json \
    && popd