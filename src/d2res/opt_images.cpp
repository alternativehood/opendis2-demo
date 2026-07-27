#include "opt_images.hpp"
#include "byte_reader.hpp"

namespace d2res {

ImageMap ImagesParser::parse(std::span<const uint8_t> data) {
    ByteReader r{data};
    ImageMap   map;

    while (r.remaining() > 1) {
        ImageBlock block;
        block.transparent_color_index = r.u8();
        block.opacity_algorithm = static_cast<int16_t>(r.u16le());
        block.size_x = r.i32le();
        block.size_y = r.i32le();

        // 1024-byte palette: 256 entries × 4 bytes BGRA
        for (std::size_t pi = 0; pi < 256; ++pi) {
            block.palette[pi][0] = r.u8(); // Blue
            block.palette[pi][1] = r.u8(); // Green
            block.palette[pi][2] = r.u8(); // Red
            block.palette[pi][3] = r.u8(); // Alpha
        }

        const int32_t     frames_count = r.i32le();
        const std::size_t block_idx = map.blocks.size();

        for (int32_t fi = 0; fi < frames_count; ++fi) {
            ImageFrame frame;
            frame.name = r.null_terminated_string();
            const int32_t pieces_count = r.i32le();
            frame.output_width = r.i32le();
            frame.output_height = r.i32le();

            frame.pieces.reserve(static_cast<std::size_t>(pieces_count));
            for (int32_t pi = 0; pi < pieces_count; ++pi) {
                ImagePiece piece;
                piece.output_x = r.i32le();
                piece.output_y = r.i32le();
                piece.base_x = r.i32le();
                piece.base_y = r.i32le();
                piece.width = r.i32le();
                piece.height = r.i32le();
                frame.pieces.push_back(piece);
            }

            // First occurrence wins for the name index
            if (!map.frame_name_to_block.contains(frame.name))
                map.frame_name_to_block[frame.name] = block_idx;

            block.frames.push_back(std::move(frame));
        }

        map.blocks.push_back(std::move(block));
    }

    return map;
}

} // namespace d2res
