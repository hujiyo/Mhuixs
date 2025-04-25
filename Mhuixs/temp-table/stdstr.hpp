#ifndef STDSTR_HPP
#define STDSTR_HPP

#include <cstring>
#include <stdexcept>

class StdStr {
private:
    char* data_;
    size_t length_;
    size_t capacity_;

    void reallocate(size_t new_capacity) {
        char* new_data = new char[new_capacity];
        if (data_) {
            memcpy(new_data, data_, length_);
            delete[] data_;
        }
        data_ = new_data;
        capacity_ = new_capacity;
    }

public:
    // Constructors
    StdStr() : data_(nullptr), length_(0), capacity_(0) {}
    
    explicit StdStr(const char* str) {
        length_ = strlen(str);
        capacity_ = length_ + 1;
        data_ = new char[capacity_];
        memcpy(data_, str, capacity_);
    }
    
    StdStr(const StdStr& other) {
        length_ = other.length_;
        capacity_ = other.capacity_;
        data_ = new char[capacity_];
        memcpy(data_, other.data_, capacity_);
    }
    
    ~StdStr() {
        delete[] data_;
    }

    // Assignment operators
    StdStr& operator=(const char* str) {
        size_t new_len = strlen(str);
        if (new_len + 1 > capacity_) {
            reallocate(new_len + 1);
        }
        memcpy(data_, str, new_len + 1);
        length_ = new_len;
        return *this;
    }
    
    StdStr& operator=(const StdStr& other) {
        if (this != &other) {
            if (other.length_ + 1 > capacity_) {
                reallocate(other.capacity_);
            }
            memcpy(data_, other.data_, other.length_ + 1);
            length_ = other.length_;
        }
        return *this;
    }

    // Capacity
    size_t size() const { return length_; }
    size_t length() const { return length_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return length_ == 0; }
    void reserve(size_t new_capacity);

    // Element access
    char& operator[](size_t pos) { return data_[pos]; }
    const char& operator[](size_t pos) const { return data_[pos]; }
    char& at(size_t pos);
    const char& at(size_t pos) const;
    char& front() { return data_[0]; }
    const char& front() const { return data_[0]; }
    char& back() { return data_[length_ - 1]; }
    const char& back() const { return data_[length_ - 1]; }
    const char* c_str() const { return data_ ? data_ : ""; }
    const char* data() const { return c_str(); }

    // Modifiers
    void clear();
    void push_back(char c);
    void pop_back();
    void append(const char* str, size_t count);
    void append(const char* str) { append(str, strlen(str)); }
    void append(const StdStr& str) { append(str.data_, str.length_); }
    StdStr& operator+=(const char* str) { append(str); return *this; }
    StdStr& operator+=(const StdStr& str) { append(str); return *this; }
    void insert(size_t pos, const char* str, size_t count);
    void insert(size_t pos, const char* str) { insert(pos, str, strlen(str)); }
    void insert(size_t pos, const StdStr& str) { insert(pos, str.data_, str.length_); }
    void erase(size_t pos = 0, size_t count = npos);
    void replace(size_t pos, size_t count, const char* str, size_t str_len);
    void replace(size_t pos, size_t count, const char* str) { replace(pos, count, str, strlen(str)); }
    void replace(size_t pos, size_t count, const StdStr& str) { replace(pos, count, str.data_, str.length_); }
    void resize(size_t new_size, char fill_char = '\0');
    void swap(StdStr& other);

    // Operations
    int compare(const char* str) const;
    int compare(const StdStr& str) const;
    size_t copy(char* dest, size_t count, size_t pos = 0) const;
    size_t find(const char* str, size_t pos = 0) const;
    size_t find(const StdStr& str, size_t pos = 0) const { return find(str.data_, pos); }
    size_t find(char c, size_t pos = 0) const;
    size_t rfind(const char* str, size_t pos = npos) const;
    size_t rfind(const StdStr& str, size_t pos = npos) const { return rfind(str.data_, pos); }
    size_t rfind(char c, size_t pos = npos) const;
    StdStr substr(size_t pos = 0, size_t count = npos) const;

    // Constants
    static const size_t npos = static_cast<size_t>(-1);

private:
    // Helper functions
    static size_t strnlen(const char* str, size_t maxlen);
};

// Non-member functions
bool operator==(const StdStr& lhs, const StdStr& rhs);
bool operator==(const StdStr& lhs, const char* rhs);
bool operator==(const char* lhs, const StdStr& rhs);
bool operator!=(const StdStr& lhs, const StdStr& rhs);
bool operator!=(const StdStr& lhs, const char* rhs);
bool operator!=(const char* lhs, const StdStr& rhs);
StdStr operator+(const StdStr& lhs, const StdStr& rhs);
StdStr operator+(const StdStr& lhs, const char* rhs);
StdStr operator+(const char* lhs, const StdStr& rhs);

#endif // STDSTR_HPP
