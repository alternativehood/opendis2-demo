# Runtime Data Tables

`AssetDatabase::get_data_table(asset_id)` returns an immutable `DataTable`.
Existing `find_data_table_by_id` and case-insensitive `find_data_table` continue
to return lightweight manifest references.

`DataTable` provides:

- exact `row_key` lookup;
- exact column-name lookup;
- lossless raw `DataValue` access;
- string, signed integer, finite floating-point, and boolean accessors;
- contextual errors containing table ID, row key, and column name.

String-to-number conversion requires the entire string to be a valid value.
Leading or trailing whitespace is not accepted. Boolean strings are exactly
`true`, `false`, `1`, or `0`. Arrays, objects, null, and incompatible scalars
return `type_conversion`; missing rows and columns return their own error codes.

The API does not infer primary keys, enums, localization, gameplay types, or
cross-asset relationships.
