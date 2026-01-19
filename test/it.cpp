#include <iostream>
#include <iterator>  // For std::iterator_traits

class IntContainer {
    int* data;
    size_t size;

public:
    IntContainer(size_t n) : size(n) {
        data = new int[n];
        for (size_t i = 0; i < n; ++i)
            data[i] = static_cast<int>(i * 10);  // Fill with sample data
    }

    ~IntContainer() { delete[] data; }

    // --- Custom Iterator Class ---
    class Iterator {
        int* ptr;  // raw pointer internally
    public:
        // Iterator traits (for STL compatibility)
        using iterator_category = std::random_access_iterator_tag;
        using value_type = int;
        using difference_type = std::ptrdiff_t;
        using pointer = int*;
        using reference = int&;

        explicit Iterator(int* p) : ptr(p) {}

        // Dereference
        reference operator*() const { return *ptr; }

        // Increment and decrement
        Iterator& operator++() { ++ptr; return *this; }
        Iterator operator++(int) { Iterator tmp(*this); ++ptr; return tmp; }

        Iterator& operator--() { --ptr; return *this; }
        Iterator operator--(int) { Iterator tmp(*this); --ptr; return tmp; }

        // Random access support
        Iterator operator+(difference_type n) const { return Iterator(ptr + n); }
        Iterator operator-(difference_type n) const { return Iterator(ptr - n); }
        difference_type operator-(const Iterator& other) const { return ptr - other.ptr; }

        // Comparisons
        bool operator==(const Iterator& other) const { return ptr == other.ptr; }
        bool operator!=(const Iterator& other) const { return ptr != other.ptr; }
        bool operator<(const Iterator& other) const { return ptr < other.ptr; }

        // Indexing
        reference operator[](difference_type n) const { return *(ptr + n); }
    };

    // begin() and end() methods
    Iterator begin() { return Iterator(data); }
    Iterator end() { return Iterator(data + size); }

    size_t getSize() const { return size; }
};

// --- Demonstration ---
int main() {
    IntContainer container(5);

    std::cout << "Using custom iterator:\n";
    for (auto it = container.begin(); it != container.end(); ++it)
        std::cout << *it << " ";
    std::cout << "\n";
    for (auto it: container) {
        std::cout << it << " ";
    }
    std::cout << "\n";

    // Random access example
    auto it = container.begin();
    std::cout << "Third element (it[2]): " << it[2] << "\n";
    std::cout << "Distance between begin() and end(): " << (container.end() - container.begin()) << "\n";

    return 0;
}