#!/bin/bash

# Set up 4 veth interfaces and vlans
ip link del dev veth0 2>/dev/null || :
ip link del dev veth1 2>/dev/null || :
ip link del dev veth2 2>/dev/null || :
ip link del dev veth3 2>/dev/null || :
ip link add veth0 type veth peer veth1
ip link add veth2 type veth peer veth3
ip link add link veth0 name veth0.295 type vlan id 295
ip link add link veth1 name veth1.295 type vlan id 295
ip link add link veth2 name veth2.295 type vlan id 295
ip link add link veth3 name veth3.295 type vlan id 295
ip link set dev veth0 up
ip link set dev veth1 up
ip link set dev veth2 up
ip link set dev veth3 up
ip link set dev veth0.295 up
ip link set dev veth1.295 up
ip link set dev veth2.295 up
ip link set dev veth3.295 up

# Give some time for the interfaces to come up
sleep 2

export LD_LIBRARY_PATH="../build"

# Run all tests from directory
tests=$(find * -type f -name 'test_*')

for f in $tests
do
    if test -x ./"$f"; then
        if ./"$f" > ./"$f".out 2> ./"$f".err; then
            echo "PASS: $f"
        else
            echo "FAIL: $f"
        fi
    fi
done

# When done, delete the peers
ip link del dev veth0 2>/dev/null || :
ip link del dev veth1 2>/dev/null || :
ip link del dev veth2 2>/dev/null || :
ip link del dev veth3 2>/dev/null || :
