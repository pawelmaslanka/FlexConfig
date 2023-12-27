#!/bin/bash

pushd build && cmake .. && make \
    && cp ../config_reference.json ../config_test.json \
    && cp ../schema_reference.json ../schema_test.json \
    && ./FlexConfig --config ../config_test.json --schema ../schema_test.json --address localhost --port 8001 \
    && popd