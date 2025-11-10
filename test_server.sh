#!/bin/bash
# Test script for neatvi server/client functionality

echo "=== Testing neatvi server/client functionality ==="
echo

# Create test files
echo "This is test file 1" > /tmp/test1.txt
echo "This is test file 2" > /tmp/test2.txt

# Test 1: Check serverlist before starting server
echo "Test 1: Check serverlist (should be empty)"
./vi --serverlist
echo

# Test 2: Start server in background
echo "Test 2: Starting server named 'TESTVI' in background..."
timeout 5 ./vi --servername TESTVI /tmp/test1.txt &
SERVER_PID=$!
sleep 1

# Test 3: Check if server is running
echo "Test 3: Check serverlist (should show TESTVI)"
./vi --serverlist
echo

# Test 4: Send a file to open
echo "Test 4: Sending file to server..."
./vi --remote /tmp/test2.txt --servername TESTVI
if [ $? -eq 0 ]; then
    echo "SUCCESS: File sent to server"
else
    echo "FAILED: Could not send file to server"
fi
echo

# Test 5: Try to send command to non-existent server
echo "Test 5: Try to send to non-existent server (should fail)"
./vi --remote /tmp/test1.txt --servername NONEXISTENT 2>&1
echo

# Cleanup
echo "Cleaning up..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
rm -f /tmp/test1.txt /tmp/test2.txt
sleep 1

# Verify server is cleaned up
echo "Test 6: Verify server cleanup"
./vi --serverlist
echo

echo "=== Tests complete ==="
