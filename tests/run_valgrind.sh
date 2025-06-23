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

# Run all tests from directory
tests=$(find * -type f -name 'test_*' ! -name 'test_session_multicast' ! -name 'test_session_lb_discovery')

for f in $tests
do
    if test -x ./"$f"; then
        valgrind --suppressions=${SCRIPT_PATH}/glibc.supp --show-error-list=yes --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 \
            ./"$f" > ./"$f".out 2> ./"$f".err
        if [ $? -eq 0 ]; then
            echo "PASS [valgrind]: $f"
        else
            echo "FAIL [valgrind]: $f"
        fi
    fi
done

# Clean up previous configuration just in case
ip link del dev veth0 2>/dev/null || :
ip link del dev veth1 2>/dev/null || :
ip link del dev veth2 2>/dev/null || :
ip link del dev veth3 2>/dev/null || :
sleep 2

# Create a network bridge
ip link add name br0 type bridge
ip link set br0 up

# Create LBM/LBR endpoints
ip link add veth-lbm type veth peer name lbm-peer
ip link add veth-lbr1 type veth peer name lbr1-peer
ip link add veth-lbr2 type veth peer name lbr2-peer
ip link add veth-lbr3 type veth peer name lbr3-peer

# Attach one end of each pair to the bridge
ip link set veth-lbm master br0
ip link set veth-lbr1 master br0
ip link set veth-lbr2 master br0
ip link set veth-lbr3 master br0

# Bring everything up
ip link set veth-lbm up
ip link set veth-lbr1 up
ip link set veth-lbr2 up
ip link set veth-lbr3 up
ip link set lbm-peer up
ip link set lbr1-peer up
ip link set lbr2-peer up
ip link set lbr3-peer up
sleep 2

# Run multicast with valgrind
test_multicast=test_session_multicast
valgrind --suppressions=${SCRIPT_PATH}/glibc.supp --show-error-list=yes --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 \
    ./${test_multicast} > ./${test_multicast}.out 2> ./${test_multicast}.err
if [ $? -eq 0 ]; then
    echo "PASS [valgrind]: ${test_multicast}"
else
    echo "FAIL [valgrind]: ${test_multicast}"
fi

# Run LB_DISCOVER with valgrind
if_meg_0=( lbr1-peer lbr2-peer lbr3-peer )
declare -A macs
for iface in "${if_meg_0[@]}"; do
    macs["$iface"]=$(ip -o link show "$iface" | awk '{print $17}')
done

test_lb_discover=test_session_lb_discovery
valgrind --suppressions=${SCRIPT_PATH}/glibc.supp --show-error-list=yes --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 \
    ./${test_lb_discover} "${macs[@]}" > ./${test_lb_discover}.out 2> ./${test_lb_discover}.err
if [ $? -eq 0 ]; then
    echo "PASS [valgrind]: ${test_lb_discover}"
else
    echo "FAIL [valgrind]: ${test_lb_discover}"
fi

# When done, clean everything up
ip link set veth0 nomaster 2>/dev/null || :
ip link set br0 down 2>/dev/null || :
ip link delete br0 type bridge 2>/dev/null || :
ip link delete veth-lbm 2>/dev/null || :
ip link delete veth-lbr1 2>/dev/null || :
ip link delete veth-lbr2 2>/dev/null || :
ip link delete veth-lbr3 2>/dev/null || :

