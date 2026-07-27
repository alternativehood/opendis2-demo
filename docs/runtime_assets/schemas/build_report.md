# build_report.json

Written to `reports/build_report.json` during runtime package construction.

```json
{
  "report_schema_version": 1,
  "package_path": ".",
  "valid": true,
  "errors": [],
  "warnings": [
    {
      "code": "unsupported_container",
      "message": "unsupported container: Maps/example.ff",
      "path": "Maps/example.ff"
    }
  ],
  "summary": {
    "error_count": 0,
    "warning_count": 1
  }
}
```

Diagnostics are sorted by severity, code, path, and message. `path` is omitted
when no package-relative or source-relative location applies. The current
`report_schema_version` is `1`.
