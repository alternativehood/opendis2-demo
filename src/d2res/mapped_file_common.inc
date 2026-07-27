MappedFile::MappedFile() = default;

MappedFile::MappedFile(MappedFile&& other) noexcept
    : native_state_(std::move(other.native_state_)), data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
    if (this != &other) {
        native_state_ = std::move(other.native_state_);
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

MappedFile::~MappedFile() = default;
