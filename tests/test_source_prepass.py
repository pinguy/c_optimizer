#!/usr/bin/env python3
"""Regression: libc indirection must not remove headers required by other calls."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
PREPASS = ROOT / "tiny_tools" / "source_prepass.py"

class HeaderRetention(unittest.TestCase):
    def test_string_uses_other_than_memset(self):
        source = """#include <string.h>
#include <stddef.h>
#include <dlfcn.h>
int main(int n, char **v) { void *h=dlopen("libc.so.6",1); dlsym(h,"x"); char a[10]; memset(a, 0, 10); return strcmp(v[0], "hi"); }
"""
        with tempfile.TemporaryDirectory() as t:
            a = pathlib.Path(t) / "input.c"
            b = pathlib.Path(t) / "output.c"
            a.write_text(source)
            subprocess.run([sys.executable, str(PREPASS), str(a), str(b)], check=True, capture_output=True)
            output = b.read_text()
            self.assertIn("#include <string.h>", output)
            self.assertIn("void* memset(", output)
            subprocess.run(["gcc", "-Werror=implicit-function-declaration", "-fsyntax-only", str(b)], check=True, capture_output=True)

    def test_string_can_be_removed_for_memset_only(self):
        source = """#include <string.h>
#include <dlfcn.h>
int main(void){void *h=dlopen("libc.so.6",1);dlsym(h,"x");char a[8];memset(a,0,8);return a[0];}
"""
        with tempfile.TemporaryDirectory() as t:
            a = pathlib.Path(t) / "input.c"
            b = pathlib.Path(t) / "output.c"
            a.write_text(source)
            subprocess.run([sys.executable, str(PREPASS), str(a), str(b)], check=True, capture_output=True)
            output = b.read_text()
            self.assertNotIn("#include <string.h>", output)
            self.assertIn("void* memset(", output)
            subprocess.run(["gcc", "-Werror=implicit-function-declaration", "-fsyntax-only", str(b)], check=True, capture_output=True)

if __name__ == "__main__":
    unittest.main(verbosity=2)
