#include "stdstr.hpp"
#include <algorithm>

void StdStr::reserve(size_t new_capacity) {
    if (new_capacity > capacity_) {
        reallocate(new_capacity);
    }
}

char& StdStr::at(size_t pos) {
    if (pos >= length_) {
        throw std::out_of_range("StdStr::at");
    }
    return data_[pos];
}

const char& StdStr::at(size_t pos) const {
    if (pos >= length_) {
        throw std::out_of_range("StdStr::at");
    }
    return data_[pos];
}

void StdStr::clear() {
    if (data_) {
        data_[0] = '\0';
    }
    length_ = 0;
}

void StdStr::push_back(char c) {
    if (length_ + 1 >= capacity_) {
        reallocate(capacity_ ? capacity_ * 2 : 16);
    }
    data_[length_] = c;
    data_[length_ + 1] = '\0';
    ++length_;
}

void StdStr::pop_back() {
    if (length_ > 0) {
        data_[--length_] = '\0';
    }
}

void StdStr::append(const char* str, size_t count) {
    if (count == 0) return;
    
    size_t new_len = length_ + count;
    if (new_len + 1 > capacity_) {
        reallocate(std::max(new_len + 1, capacity_ * 2));
    }
    memcpy(data_ + length_, str, count);
    length_ = new_len;
    data_[length_] = '\0';
}

void StdStr::insert(size_t pos, const char* str, size_t count) {
    if (pos > length_) {
        throw std::out_of_range("StdStr::insert");
    }
    if (count == 0) return;
    
    size_t new_len = length_ + count;
    if (new_len + 1 > capacity_) {
        reallocate(std::max(new_len + 1, capacity_ * 2));
    }
    
    memmove(data_ + pos + count, data_ + pos, length_ - pos);
    memcpy(data_ + pos, str, count);
    length_ = new_len;
    data_[length_] = '\0';
}

void StdStr::erase(size_t pos, size_t count) {
    if (pos > length_) {
        throw std::out_of_range("StdStr::erase");
    }
    count = std::min(count, length_ - pos);
    if (count == 0) return;
    
    memmove(data_ + pos, data_ + pos + count, length_ - pos - count + 1);
    length_ -= count;
}

void StdStr::replace(size_t pos, size_t count, const char* str, size_t str_len) {
    if (pos > length_) {
        throw std::out_of_range("StdStr::replace");
    }
    count = std::min(count, length_ - pos);
    
    size_t new_len = length_ - count + str_len;
    if (new_len + 1 > capacity_) {
        reallocate(std::max(new_len + 1, capacity_ * 2));
    }
    
    if (str_len != count) {
        memmove(data_ + pos + str_len, data_ + pos + count, length_ - pos - count + 1);
    }
    memcpy(data_ + pos, str, str_len);
    length_ = new_len;
}

void StdStr::resize(size_t new_size, char fill_char) {
    if (new_size < length_) {
        data_[new_size] = '\0';
        length_ = new_size;
    } else if (new_size > length_) {
        if (new_size + 1 > capacity_) {
            reallocate(std::max(new_size + 1, capacity_ * 2));
        }
        memset(data_ + length_, fill_char, new_size - length_);
        data_[new_size] = '\0';
        length_ = new_size;
    }
}

void StdStr::swap(StdStr& other) {
    std::swap(data_, other.data_);
    std::swap(length_, other.length_);
    std::swap(capacity_, other.capacity_);
}

int StdStr::compare(const char* str) const {
    return strcmp(c_str(), str);
}

int StdStr::compare(const StdStr& str) const {
    return strcmp(c_str(), str.c_str());
}

size_t StdStr::copy(char* dest, size_t count, size_t pos) const {
    if (pos > length_) {
        throw std::out_of_range("StdStr::copy");
    }
    count = std::min(count, length_ - pos);
    memcpy(dest, data_ + pos, count);
    return count;
}

size_t StdStr::find(const char* str, size_t pos) const {
    if (pos > length_) return npos;
    const char* found = strstr(data_ + pos, str);
    return found ? found - data_ : npos;
}

size_t StdStr::find(char c, size_t pos) const {
    if (pos > length_) return npos;
    const char* found = static_cast<const char*>(memchr(data_ + pos, c, length_ - pos));
    return found ? found - data_ : npos;
}

size_t StdStr::rfind(const char* str, size_t pos) const {
    size_t str_len = strlen(str);
    if (str_len > length_) return npos;
    pos = std::min(pos, length_ - str_len);
    
    for (size_t i = pos + 1; i-- > 0; ) {
        if (memcmp(data_ + i, str, str_len) == 0) {
            return i;
        }
    }
    return npos;
}

size_t StdStr::rfind(char c, size_t pos) const {
    pos = std::min(pos, length_ - 1);
    for (size_t i = pos + 1; i-- > 0; ) {
        if (data_[i] == c) {
            return i;
        }
    }
    return npos;
}

StdStr StdStr::substr(size_t pos, size_t count) const {
    if (pos > length_) {
        throw std::out_of_range("StdStr::substr");
    }
    count = std::min(count, length_ - pos);
    StdStr result;
    result.append(data_ + pos, count);
    return result;
}

size_t StdStr::strnlen(const char* str, size_t maxlen) {
    const char* end = static_cast<const char*>(memchr(str, '\0', maxlen));
    return end ? end - str : maxlen;
}

// Non-member functions
bool operator==(const StdStr& lhs, const StdStr& rhs) {
    return lhs.compare(rhs) == 0;
}

bool operator==(const StdStr& lhs, const char* rhs) {
    return lhs.compare(rhs) == 0;
}

bool operator==(const char* lhs, const StdStr& rhs) {
    return rhs.compare(lhs) == 0;
}

bool operator!=(const StdStr& lhs, const StdStr& rhs) {
    return !(lhs == rhs);
}

bool operator!=(const StdStr& lhs, const char* rhs) {
    return !(lhs == rhs);
}

bool operator!=(const char* lhs, const StdStr& rhs) {
    return !(lhs == rhs);
}

StdStr operator+(const StdStr& lhs, const StdStr& rhs) {
    StdStr result(lhs);
    result += rhs;
    return result;
}

StdStr operator+(const StdStr& lhs, const char* rhs) {
    StdStr result(lhs);
    result += rhs;
    return result;
}

StdStr operator+(const char* lhs, const StdStr& rhs) {
    StdStr result(lhs);
    result += rhs;
    return result;
}
