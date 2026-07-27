// Research helper for dBASE III DBF inspection.
// Provenance: DBF header/field descriptor layout used for manual checks only.

struct DbfHeader {
    u8 version;
    u8 yy;
    u8 mm;
    u8 dd;
    u32 record_count;
    u16 header_size;
    u16 record_size;
    u8 reserved[20];
};

struct DbfFieldDescriptor {
    char name[11];
    char type;
    u32 address;
    u8 length;
    u8 decimal_count;
    u8 reserved[14];
};

DbfHeader header @ 0x0;
