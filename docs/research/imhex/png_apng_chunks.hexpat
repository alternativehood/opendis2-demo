// Research helper for PNG/APNG chunk inspection.
// Provenance: PNG/APNG public chunk layout, used only as manual research aid.

struct PngSignature {
    u8 bytes[8];
};

struct PngChunk {
    u32be length;
    char type[4];
    u8 data[length];
    u32be crc;
};

PngSignature signature @ 0x0;
