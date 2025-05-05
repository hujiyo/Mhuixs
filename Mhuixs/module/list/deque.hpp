#include <cstddef>
#include <new>
#include <cstdlib>

template <typename T>
class IntDeque {
private:
    static const size_t BlockSize = 1024; // 每个块存储1024个int

    struct Block {
        T data[BlockSize];
        Block* prev;
        Block* next;
    };

    Block* head;       // 双向链表的头节点
    Block* tail;       // 双向链表的尾节点
    Block* front_block; // 当前第一个元素所在块
    size_t front_offset; // front_block中的偏移
    Block* back_block;  // 当前最后一个元素的下一个位置所在块
    size_t back_offset; // back_block中的偏移
    size_t size_;       // 元素总数

public:
    IntDeque() : head(nullptr), tail(nullptr), front_block(nullptr),
                 front_offset(0), back_block(nullptr), back_offset(0),
                 size_(0) {}

    ~IntDeque() {
        clear();
    }

    void push_back(const T& value) {
        if (!back_block) {
            // 初始插入
            back_block = new Block();
            back_block->prev = nullptr;
            back_block->next = nullptr;
            head = back_block;
            tail = back_block;
            front_block = back_block;
            front_offset = BlockSize - 1; // 初始front_offset设为末尾
            back_offset = 0;
        }

        if (back_offset < BlockSize) {
            back_block->data[back_offset] = value;
            back_offset++;
        } else {
            // 新增块到尾部
            Block* new_block = new Block();
            new_block->prev = tail;
            new_block->next = nullptr;
            tail->next = new_block;
            tail = new_block;
            back_block = new_block;
            back_offset = 1;
            back_block->data[0] = value;
        }
        size_++;
    }

    void push_front(const T& value) {
        if (!front_block) {
            // 初始插入
            front_block = new Block();
            front_block->prev = nullptr;
            front_block->next = nullptr;
            head = front_block;
            tail = front_block;
            back_block = front_block;
            front_offset = BlockSize - 1;
            back_offset = 0;
            front_block->data[front_offset] = value;
            size_++;
            return;
        }

        if (front_offset > 0) {
            front_offset--;
            front_block->data[front_offset] = value;
        } else {
            // 新增块到头部
            Block* new_block = new Block();
            new_block->next = front_block;
            new_block->prev = nullptr;
            front_block->prev = new_block;
            front_block = new_block;
            head = new_block;
            front_offset = BlockSize - 1;
            new_block->data[front_offset] = value;
        }
        size_++;
    }

    void pop_front() {
        if (size_ == 0) throw std::out_of_range("deque is empty");
        if (front_offset < BlockSize - 1) {
            front_offset++;
        } else {
            // 删除当前front_block并移动到下一个块
            Block* old_block = front_block;
            front_block = front_block->next;
            if (front_block) {
                front_block->prev = nullptr;
            }
            delete old_block;
            if (!front_block) {
                back_block = nullptr;
                back_offset = 0;
                front_offset = 0;
            } else {
                front_offset = 0;
            }
            head = front_block;
        }
        size_--;
    }

    void pop_back() {
        if (size_ == 0) throw std::out_of_range("deque is empty");
        if (back_offset > 0) {
            back_offset--;
        } else {
            // 删除当前back_block并移动到前一个块
            Block* old_block = back_block;
            back_block = back_block->prev;
            if (back_block) {
                back_block->next = nullptr;
            }
            delete old_block;
            if (!back_block) {
                front_block = nullptr;
                front_offset = 0;
            } else {
                back_offset = BlockSize;
            }
            tail = back_block;
        }
        size_--;
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    T& front() {
        if (empty()) throw std::out_of_range("empty deque");
        return front_block->data[front_offset];
    }

    const T& front() const {
        if (empty()) throw std::out_of_range("empty deque");
        return front_block->data[front_offset];
    }

    T& back() {
        if (empty()) throw std::out_of_range("empty deque");
        return back_block->data[back_offset - 1];
    }

    const T& back() const {
        if (empty()) throw std::out_of_range("empty deque");
        return back_block->data[back_offset - 1];
    }

    void clear() {
        while (!empty()) {
            pop_back();
        }
        Block* current = head;
        while (current) {
            Block* next = current->next;
            delete current;
            current = next;
        }
        head = nullptr;
        tail = nullptr;
        front_block = nullptr;
        back_block = nullptr;
        front_offset = 0;
        back_offset = 0;
        size_ = 0;
    }
};