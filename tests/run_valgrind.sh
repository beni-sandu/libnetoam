#!/bin/bash

SCRIPT_PATH=$(dirname $0)

# Check if valgrind binary exists
if ! which valgrind &> /dev/null; then
    echo "Error: valgrind is not installed or not in PATH."
    exit 1
fi

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

#--show-leak-kinds=all

# Run all tests from directory
tests=test_valgrind

for f in $tests
do
    if test -x ./"$f"; then
        valgrind --suppressions=${SCRIPT_PATH}/glibc.supp --show-error-list=yes --leak-check=full --track-origins=yes --error-exitcode=1 ./"$f" > ./"$f".out 2> ./"$f".err
        if [ $? -eq 0 ]; then
            echo "PASS: $f"
        else
            echo "FAIL: $f"
        fi
    fi
done

# When done, remove the peers
ip link del dev veth0 2>/dev/null || :
ip link del dev veth1 2>/dev/null || :
ip link del dev veth2 2>/dev/null || :
ip link del dev veth3 2>/dev/null || :
