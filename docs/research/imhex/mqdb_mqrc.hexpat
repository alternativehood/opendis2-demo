// Research helper for manual MQDB/MQRC inspection.
// Provenance: project-local notes from OpenDis2 parser fields; not a runtime parser source.

struct MqdbHeader {
    char magic[4];
    u32 unknown0;
    u32 record_count_hint;
    u32 unknown1;
    u32 unknown2;
    u32 unknown3;
    u32 checksum;
};

struct MqrcRecord {
    char magic[4];
    u32 unknown0;
    s32 id;
    s32 size;
    s32 real_file_size;
    s32 is_not_deleted;
    s32 record_magic;
    u8 payload[real_file_size];
};

MqdbHeader header @ 0x0;
