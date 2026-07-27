# Asset Reference Resolution Policy

Confirmed links require an exact canonical asset ID of the expected type or a
unique exact logical-name match. Heuristic links currently use the normalized
unit-ID to animation-name prefix rule and confidence 80.

Examples:

- `g000uu0001` to `G000UU0001IDLEA1A00`: heuristic idle-animation link;
- a DLG image value `Portrait` with one image match: confirmed dialog-image link;
- two `Portrait` image assets: unresolved `ambiguous` with sorted candidates;
- an image field containing an animation ID: unresolved `wrong_type`;
- an unknown symbolic sound trigger: unresolved `unsupported_mapping`.

Resolver rules run in fixed order and serialize links, unresolved records,
candidates, warnings, and evidence deterministically.
