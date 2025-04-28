#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <string>

class Buffer {
public:
    Buffer() = default;
    explicit Buffer(const std::string& data) : data_(data.begin(), data.end()) {}
    
    std::vector<char>& getData() { return data_; }
    const std::vector<char>& getData() const { return data_; }
    
private:
    std::vector<char> data_;
};

#endif // BUFFER_H 