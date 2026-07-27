#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <span>

namespace d2engine {

enum class FsrPass {
    Easu,
    Rcas,
};

struct EmbeddedShaderDescriptor {
    std::span<const std::uint8_t> code;
    const char*                   entrypoint = nullptr;
    SDL_GPUShaderFormat           format = SDL_GPU_SHADERFORMAT_INVALID;
    FsrPass                       pass = FsrPass::Easu;
    std::uint32_t                 num_samplers = 0;
    std::uint32_t                 num_uniform_buffers = 0;
};

[[nodiscard]] const EmbeddedShaderDescriptor& embedded_fsr_shader(FsrPass             pass,
                                                                  SDL_GPUShaderFormat format);
[[nodiscard]] const char*                     fsr_pass_name(FsrPass pass) noexcept;

} // namespace d2engine
