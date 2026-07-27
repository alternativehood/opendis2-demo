#include "fsr_embedded_shader.hpp"

#include "fsr_easu_dxil.hpp"
#include "fsr_easu_msl.hpp"
#include "fsr_easu_spv.hpp"
#include "fsr_rcas_dxil.hpp"
#include "fsr_rcas_msl.hpp"
#include "fsr_rcas_spv.hpp"

#include <array>
#include <stdexcept>

namespace d2engine {
namespace {

const std::array<EmbeddedShaderDescriptor, 6> kEmbeddedShaders{{
    {.code = fsr_shader_binaries::fsr_easu_spv,
     .entrypoint = "main",
     .format = SDL_GPU_SHADERFORMAT_SPIRV,
     .pass = FsrPass::Easu,
     .num_samplers = 1U,
     .num_uniform_buffers = 1U},
    {.code = fsr_shader_binaries::fsr_rcas_spv,
     .entrypoint = "main",
     .format = SDL_GPU_SHADERFORMAT_SPIRV,
     .pass = FsrPass::Rcas,
     .num_samplers = 1U,
     .num_uniform_buffers = 1U},
    {.code = fsr_shader_binaries::fsr_easu_dxil,
     .entrypoint = "main",
     .format = SDL_GPU_SHADERFORMAT_DXIL,
     .pass = FsrPass::Easu,
     .num_samplers = 1U,
     .num_uniform_buffers = 1U},
    {.code = fsr_shader_binaries::fsr_rcas_dxil,
     .entrypoint = "main",
     .format = SDL_GPU_SHADERFORMAT_DXIL,
     .pass = FsrPass::Rcas,
     .num_samplers = 1U,
     .num_uniform_buffers = 1U},
    {.code = fsr_shader_binaries::fsr_easu_msl,
     .entrypoint = "main0",
     .format = SDL_GPU_SHADERFORMAT_MSL,
     .pass = FsrPass::Easu,
     .num_samplers = 1U,
     .num_uniform_buffers = 1U},
    {.code = fsr_shader_binaries::fsr_rcas_msl,
     .entrypoint = "main0",
     .format = SDL_GPU_SHADERFORMAT_MSL,
     .pass = FsrPass::Rcas,
     .num_samplers = 1U,
     .num_uniform_buffers = 1U},
}};

} // namespace

// cppcheck-suppress unusedFunction
const EmbeddedShaderDescriptor& embedded_fsr_shader(FsrPass pass, SDL_GPUShaderFormat format) {
    for (const auto& descriptor : kEmbeddedShaders) {
        if (descriptor.pass == pass && descriptor.format == format) {
            return descriptor;
        }
    }
    throw std::runtime_error("Embedded FSR shader descriptor is unavailable");
}

// cppcheck-suppress unusedFunction
const char* fsr_pass_name(FsrPass pass) noexcept {
    switch (pass) {
    case FsrPass::Easu:
        return "EASU";
    case FsrPass::Rcas:
        return "RCAS";
    }
    return "unknown";
}

} // namespace d2engine
