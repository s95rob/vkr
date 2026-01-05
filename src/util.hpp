#pragma once

#include <fstream>
#include <vector>
#include <cstdint>

class FileReader {
public:
    FileReader() = default;
    FileReader(const char* filepath) {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);

        if (file.is_open()) {
            size_t length = (size_t)file.tellg();
            m_buf.resize(length);

            file.seekg(0);
            file.read(reinterpret_cast<char*>(m_buf.data()), length);
        }
    }

    void* Data() { return reinterpret_cast<void*>(m_buf.data()); }
    size_t Size() const { return m_buf.size(); }

    operator bool() const { return !m_buf.empty(); }

private:
    std::vector<uint8_t> m_buf;
};