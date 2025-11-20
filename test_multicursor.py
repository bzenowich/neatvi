#!/usr/bin/env python3
"""
Automated test for multi-cursor feature in neatvi
Uses pexpect to interact with the editor
"""

import os
import sys
import tempfile
import shutil

try:
    import pexpect
except ImportError:
    print("Error: pexpect module not found")
    print("Install with: pip3 install pexpect")
    sys.exit(1)

# ANSI color codes
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
NC = '\033[0m'

class TestRunner:
    def __init__(self):
        self.test_num = 0
        self.passed = 0
        self.failed = 0
        self.test_dir = tempfile.mkdtemp(prefix='neatvi_test_')

    def cleanup(self):
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)

    def run_test(self, name, input_content, commands, expected_output):
        """Run a single test case"""
        self.test_num += 1
        print(f"Test {self.test_num}: {name} ... ", end='', flush=True)

        # Create test file
        test_file = os.path.join(self.test_dir, f'test_{self.test_num}.txt')
        with open(test_file, 'w') as f:
            f.write(input_content)

        try:
            # Start neatvi
            child = pexpect.spawn('./vi', [test_file], timeout=5,
                                 encoding='utf-8', cwd=os.getcwd())

            # Send commands
            for cmd in commands:
                child.send(cmd)
                child.expect_list([pexpect.TIMEOUT, pexpect.EOF], timeout=0.1)

            # Wait for exit
            child.expect(pexpect.EOF)
            child.close()

            # Read result
            with open(test_file, 'r') as f:
                result = f.read()

            # Compare
            if result == expected_output:
                print(f"{GREEN}PASS{NC}")
                self.passed += 1
            else:
                print(f"{RED}FAIL{NC}")
                print(f"  Expected: {repr(expected_output)}")
                print(f"  Got:      {repr(result)}")
                self.failed += 1

        except Exception as e:
            print(f"{RED}ERROR{NC}")
            print(f"  Exception: {e}")
            self.failed += 1

    def print_summary(self):
        print()
        print("=" * 40)
        print("  Test Results")
        print("=" * 40)
        print(f"Tests run:    {self.test_num}")
        print(f"Tests passed: {self.passed}")
        print(f"Tests failed: {self.failed}")

        if self.failed == 0:
            print(f"{GREEN}All tests passed!{NC}")
            return 0
        else:
            print(f"{RED}Some tests failed!{NC}")
            return 1

def main():
    runner = TestRunner()

    try:
        print("=" * 40)
        print("  Multi-Cursor Feature Test Suite")
        print("=" * 40)
        print()

        # Test 1: Basic two-cursor insert
        runner.run_test(
            "Insert at two positions",
            "line1\nline2\nline3\n",
            ["gg", "0", "\x0E", "j", "0", "\x0E", "i", "XXX", "\x1B", ":wq\n"],
            "XXXline1\nXXXline2\nline3\n"
        )

        # Test 2: Append at two positions
        runner.run_test(
            "Append at two positions",
            "abc\ndef\nghi\n",
            ["gg", "$", "\x0E", "j", "$", "\x0E", "a", " END", "\x1B", ":wq\n"],
            "abc END\ndef END\nghi\n"
        )

        # Test 3: Clear cursors with Ctrl-\
        runner.run_test(
            "Clear cursors with Ctrl-\\",
            "line1\nline2\n",
            ["gg", "0", "\x0E", "j", "0", "\x0E", "\x1C", "i", "TEST", "\x1B", ":wq\n"],
            "line1\nTESTline2\n"  # After clearing, cursor is still at line 2
        )

        # Test 4: Three cursor positions
        runner.run_test(
            "Three cursor positions",
            "aaa\nbbb\nccc\nddd\n",
            ["gg", "0", "\x0E", "j", "0", "\x0E", "j", "0", "\x0E", "i", "[X]", "\x1B", ":wq\n"],
            "[X]aaa\n[X]bbb\n[X]ccc\nddd\n"
        )

        # Test 5: Insert at line beginning with I
        runner.run_test(
            "Insert at beginning with I",
            "  tab1\n  tab2\n  tab3\n",
            ["gg", "\x0E", "j", "\x0E", "I", "#", "\x1B", ":wq\n"],
            "  #tab1\n  #tab2\n  tab3\n"  # I inserts at first non-whitespace
        )

        # Test 6: Append at line end with A
        runner.run_test(
            "Append at end with A",
            "one\ntwo\nthree\n",
            ["gg", "\x0E", "j", "\x0E", "A", ";", "\x1B", ":wq\n"],
            "one;\ntwo;\nthree\n"
        )

        # Test 7: Multiple positions on same line
        runner.run_test(
            "Multiple cursors same line",
            "abcdefgh\n",
            ["gg", "0", "\x0E", "3l", "\x0E", "6l", "\x0E", "i", "*", "\x1B", ":wq\n"],
            "*abc*defg*h\n"  # Cursors at 0, 3, 9(clamped to 8) -> insert adjusts positions
        )

        return runner.print_summary()

    finally:
        runner.cleanup()

if __name__ == '__main__':
    sys.exit(main())
