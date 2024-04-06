#!/bin/bash

pushd build && cmake .. && make \
    && cp ../config_reference.json ../config_test.json \
    && cp ../schema_reference.json ../schema/root.json \
    && ./FlexConfig --config ../config_test.json --schema ../schema/root.json --address localhost --port 8001 \
    && popd
