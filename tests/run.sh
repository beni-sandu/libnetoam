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
tests=$(find * -type f -name 'test_*' ! -name 'test_session_run' ! -name 'test_session_multicast' ! -name 'test_session_lb_discovery')

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

# Below is a testcase for multicast sessions
# We need to create a network setup that simulates a switch

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

# Run the test binary
test_multicast=test_session_multicast
./${test_multicast} > ./${test_multicast}.out 2> ./${test_multicast}.err

if_meg_0=( lbr1-peer lbr2-peer )
if_meg_1=( lbr3-peer )

# Get MACs
declare -A macs
for iface in "${if_meg_0[@]}" "${if_meg_1[@]}"; do
    macs["$iface"]=$(ip -o link show "$iface" | awk '{print $17}')
done

# Check LBR replies
log="${test_multicast}.out"
expected_ok=1
for iface in "${if_meg_0[@]}"; do
    mac=${macs[$iface]}
    if ! grep -qi "Got LBR from: ${mac}," "$log"; then
        expected_ok=0
    fi
done

unexpected_found=0
for iface in "${if_meg_1[@]}"; do
    mac=${macs[$iface]}
    if grep -qi "Got LBR from: ${mac}," "$log"; then
        unexpected_found=1
    fi
done

if (( expected_ok && ! unexpected_found )); then
    echo "PASS: ${test_multicast}"
else
    echo "FAIL: ${test_multicast}"
fi

# Test case for LB_DISCOVER session type
if_meg_0=( lbr1-peer lbr2-peer lbr3-peer )
declare -A macs
for iface in "${if_meg_0[@]}"; do
    macs["$iface"]=$(ip -o link show "$iface" | awk '{print $17}')
done

test_lb_discover=test_session_lb_discovery

./"$test_lb_discover" "${macs[@]}" \
    > "${test_lb_discover}.out" \
    2> "${test_lb_discover}.err"

log="${test_lb_discover}.out"
all_ok=1

for iface in "${if_meg_0[@]}"; do
    mac="${macs[$iface]}"
    if ! grep -qi "Got LBR from: ${mac}," "$log"; then
        all_ok=0
    fi
done

if (( all_ok )); then
    echo "PASS: ${test_lb_discover}"
else
    echo "FAIL: ${test_lb_discover}"
fi

# When done, clean everything up
ip link set veth0 nomaster 2>/dev/null || :
ip link set br0 down 2>/dev/null || :
ip link delete br0 type bridge 2>/dev/null || :
ip link delete veth-lbm 2>/dev/null || :
ip link delete veth-lbr1 2>/dev/null || :
ip link delete veth-lbr2 2>/dev/null || :
ip link delete veth-lbr3 2>/dev/null || :

