# validation_report.json

Written by package builds and by
`opendis2-dev-extractor validate-runtime-assets <asset_root> [--report <path>]`. The default
location is `reports/validation_report.json`.

Fields:

| Field | Type | Meaning |
|---|---|---|
| `report_schema_version` | integer | Report contract version, currently `1` |
| `package_path` | string | `"."`, denoting the package root without embedding an absolute path |
| `valid` | boolean | True only when `errors` is empty |
| `errors` | array | Structural failures with stable `code`, `message`, and optional `path` |
| `warnings` | array | Recoverable issues using the same diagnostic shape |
| `summary.error_count` | integer | Number of errors |
| `summary.warning_count` | integer | Number of warnings |

Typical error codes include `missing_required_entry`, `missing_asset_file`,
`unsafe_asset_path`, `malformed_animation`, `malformed_atlas`, and
`duplicate_id`. Runtime `AssetErrorCode` values are converted to stable
snake-case report codes.
# Reference Counts

The deterministic `summary` object includes `asset_link_count` and
`unresolved_asset_link_count` in addition to error and warning counts.
