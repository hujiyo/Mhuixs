#include "stream.hpp"
#include "nlohmann/json.hpp"

STREAM::STREAM(uint32_t initcap)
    : offset(NULL_OFFSET), size(0), capacity(initcap), state(0)
{
    offset = g_memap.smalloc(initcap);
    if (offset == NULL_OFFSET) { state++; return; }
}

STREAM::~STREAM() {
    if (offset != NULL_OFFSET) g_memap.sfree(offset, capacity);
    offset = NULL_OFFSET; size = 0; capacity = 0;
}

int STREAM::ensure_capacity(uint32_t mincap) {
    if (mincap <= capacity) return 0;
    uint32_t newcap = capacity * 2;
    if (newcap < mincap) newcap = mincap;
    OFFSET new_offset = g_memap.smalloc(newcap);
    if (new_offset == NULL_OFFSET) { state++; return merr; }
    if (offset != NULL_OFFSET && size > 0) {
        memcpy(g_memap.addr(new_offset), g_memap.addr(offset), size);
        g_memap.sfree(offset, capacity);
    }
    offset = new_offset;
    capacity = newcap;
    return 0;
}

int STREAM::append(const uint8_t* buf, uint32_t len) {
    if (!buf || len == 0) return 0;
    if (ensure_capacity(size + len) != 0) return merr;
    memcpy(g_memap.addr(offset) + size, buf, len);
    size += len;
    return 0;
}

int STREAM::append_pos(uint32_t pos, const uint8_t* buf, uint32_t len) {
    if (!buf || len == 0) return 0;
    if (pos > size) pos = size;
    if (ensure_capacity(size + len) != 0) return merr;
    if (pos < size) memmove(g_memap.addr(offset) + pos + len, g_memap.addr(offset) + pos, size - pos);
    memcpy(g_memap.addr(offset) + pos, buf, len);
    size += len;
    return 0;
}

int STREAM::get(uint32_t pos, uint32_t len, uint8_t* out) {
    if (!out || pos >= size) return merr;
    uint32_t real_len = (pos + len > size) ? (size - pos) : len;
    memcpy(out, g_memap.addr(offset) + pos, real_len);
    return real_len;
}

int STREAM::set(uint32_t pos, const uint8_t* buf, uint32_t len) {
    if (!buf || len == 0) return 0;
    if (ensure_capacity(pos + len) != 0) return merr;
    memcpy(g_memap.addr(offset) + pos, buf, len);
    if (pos + len > size) size = pos + len;
    return 0;
}

int STREAM::set_char(uint32_t pos, uint32_t len, uint8_t ch) {
    if (ensure_capacity(pos + len) != 0) return merr;
    memset(g_memap.addr(offset) + pos, ch, len);
    if (pos + len > size) size = pos + len;
    return 0;
}

uint32_t STREAM::len() const { return size; }

int STREAM::iserr() const { return state; }

STREAM::STREAM(const STREAM& other)
    : offset(NULL_OFFSET), size(0), capacity(other.capacity), state(0)
{
    offset = g_memap.smalloc(other.capacity);
    if (offset == NULL_OFFSET) { state++; return; }
    if (other.size > 0) {
        memcpy(g_memap.addr(offset), g_memap.addr(other.offset), other.size);
        size = other.size;
    }
}

STREAM& STREAM::operator=(const STREAM& other) {
    if (this == &other) return *this;
    if (offset != NULL_OFFSET) g_memap.sfree(offset, capacity);
    offset = g_memap.smalloc(other.capacity);
    if (offset == NULL_OFFSET) {
        state++;
        size = 0;
        capacity = 0;
        return *this;
    }
    capacity = other.capacity;
    size = other.size;
    state = 0;
    if (other.size > 0) {
        memcpy(g_memap.addr(offset), g_memap.addr(other.offset), other.size);
    }
    return *this;
}

nlohmann::json STREAM::get_all_info() const {
    nlohmann::json info;
    info["length"] = size;
    info["capacity"] = capacity;
    return info;
}
