#!/usr/bin/env python3
"""Parse CTest Test.xml (JUnit or CTest native format) and report timings.

Invoked via:
  make test-timings
  make test-integration-timings
"""
import glob
import os
import sys
import xml.etree.ElementTree as ET

from make_guard import require_make_target
require_make_target("test-timings", "test-integration-timings")

target = sys.argv[1] if len(sys.argv) > 1 else "build/dev/Testing"

if os.path.isfile(target):
    xml_file = target
    print(f"Parsing: {xml_file}")
elif os.path.isdir(target):
    xml_files = sorted(
        glob.glob(os.path.join(target, "*/Test.xml")),
        key=os.path.getmtime,
        reverse=True,
    )
    if not xml_files:
        print(f"No Test.xml found in {target}", file=sys.stderr)
        sys.exit(1)
    xml_file = xml_files[0]
    print(
        f"WARNING: directory scan may report stale results from a prior "
        f"run.\n"
        f"  Parsing: {xml_file}"
    )
else:
    print(f"Not found: {target}", file=sys.stderr)
    sys.exit(1)

root = ET.parse(xml_file)

results = []
root_tag = root.getroot().tag

if root_tag == "testsuite" or root_tag == "testsuites":
    suite = root.getroot()
    if root_tag == "testsuites":
        suite = suite.find("testsuite")
    if suite is not None:
        for tc in suite.findall("testcase"):
            name = tc.get("name", "")
            time_str = tc.get("time", "0")
            secs = float(time_str)
            status = tc.get("status", "run")
            if tc.find("failure") is not None:
                status = "FAILED"
            results.append((secs, name, status))
else:
    for t in root.findall(".//Test"):
        name = t.get("Name", "")
        status = t.get("Status", "")
        t2 = t.find("Results/NamedMeasurement[@name=\"Execution Time\"]/Value")
        secs = float(t2.text) if t2 is not None and t2.text else 0.0
        results.append((secs, name, status))

if not results:
    print("No test results found", file=sys.stderr)
    sys.exit(1)

total = sum(r[0] for r in results)
print(f"Total: {total:.2f}s  ({len(results)} tests)")

results.sort(key=lambda x: x[0], reverse=True)

print("\nTop 20 slowest:")
for secs, name, status in results[:20]:
    print(f"  {secs:6.2f}s  {status:8s}  {name}")

over1 = [r for r in results if r[0] > 1.0]
over2 = [r for r in results if r[0] > 2.0]
if over1:
    print(f"\nTests > 1s ({len(over1)}):")
    for secs, name, _ in over1:
        print(f"  {secs:6.2f}s  {name}")
if over2:
    print(f"\nTests > 2s ({len(over2)}):")
    for secs, name, _ in over2:
        print(f"  {secs:6.2f}s  {name}")
