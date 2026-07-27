#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace d2asset {

enum class AssetErrorCode : std::uint8_t {
    MissingManifest,
    InvalidJson,
    UnsupportedSchema,
    MalformedEntry,
    DuplicateId,
    UnsafePath,
    MissingFile,
    MalformedAtlas,
    DuplicateAtlasEntry,
    InvalidAtlasRectangle,
    MissingAtlasSheet,
    MalformedAnimation,
    MalformedSound,
    UnsupportedSoundSchema,
    MissingSoundPayload,
    SoundPayloadSizeMismatch,
    MalformedDataTable,
    UnsupportedDataTableSchema,
    MalformedDataValue,
    DuplicateDataRow,
    UnsupportedAssetLinksSchema,
    MalformedAssetLink,
    DuplicateAssetLink,
};

class AssetError final : public std::runtime_error {
public:
    AssetError(AssetErrorCode code, const std::string& message, std::string context = {},
               std::filesystem::path path = {})
        : std::runtime_error(message), code_(code), context_(std::move(context)),
          path_(std::move(path)) {}

    [[nodiscard]] AssetErrorCode               code() const noexcept { return code_; }
    [[nodiscard]] const std::string&           context() const noexcept { return context_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    AssetErrorCode        code_;
    std::string           context_;
    std::filesystem::path path_;
};

} // namespace d2asset
