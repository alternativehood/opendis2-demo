# Asset Links Schema

Root `asset_links.json` uses `asset_links_schema_version: 1` and contains
`links`, `unresolved`, sorted `warnings`, and reserved `extensions`.

A source is either `{"kind":"asset","asset_id":"..."}` or
`{"kind":"data_row","table_asset_id":"...","row_key":"..."}`. Version 1
targets are canonical asset IDs only.

Each link contains `source`, `target_asset_id`, `link_kind`, `resolution`,
integer `confidence` from 0 through 100, `reason_code`, and ordered `evidence`.
Resolution is `confirmed` or `heuristic`. Evidence entries contain `field`,
`source_value`, and `target_value`.

Unresolved entries contain `source`, `link_kind`, `reason`, `reason_code`,
ordered `evidence`, and sorted unique `candidate_asset_ids`. Reasons are
`no_candidate`, `ambiguous`, `wrong_type`, and `unsupported_mapping`.

Links are ordered by source endpoint, link kind, target ID, resolution,
confidence, and reason code. Their identity is source endpoint plus link kind
plus target ID. Duplicate identities and simultaneous successful/unresolved
records for one source and kind are invalid.

Confirmed links use exact canonical IDs or unique exact logical-name evidence.
The documented normalized unit-ID animation-prefix rule is heuristic with
confidence 80. It preserves canonical `UU` IDs and accepts legacy `UN` values
by normalizing that marker to `UU`.
Unsupported symbolic sound triggers remain unresolved until canonical WDT
mapping is available.

Changes to required fields, enum meaning, identity, or ordering require a new
schema version.
