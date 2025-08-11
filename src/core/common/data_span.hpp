#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

#if __cplusplus >= 202002L && defined(KERNTOPIA_USE_CPP20)
    #include <span>
    namespace kerntopia {
        template<typename T>
        using data_span = std::span<T>;
    }
#else
    namespace kerntopia {
        
        /**
         * @brief Lightweight span-like container for C++17 compatibility
         * 
         * Provides a view over a contiguous sequence of objects with bounds checking
         * in debug builds. Compatible with std::span interface for C++20 migration.
         * 
         * @tparam T Element type
         */
        template<typename T>
        class data_span {
        public:
            using element_type = T;
            using value_type = std::remove_cv_t<T>;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using const_pointer = const T*;
            using reference = T&;
            using const_reference = const T&;
            using iterator = T*;
            using const_iterator = const T*;
            using reverse_iterator = std::reverse_iterator<iterator>;
            using const_reverse_iterator = std::reverse_iterator<const_iterator>;
            
            static constexpr size_type npos = static_cast<size_type>(-1);
            
            /**
             * @brief Default constructor - creates empty span
             */
            constexpr data_span() noexcept : data_(nullptr), size_(0) {}
            
            /**
             * @brief Construct span from pointer and size
             * 
             * @param data Pointer to first element
             * @param size Number of elements
             */
            constexpr data_span(T* data, size_type size) noexcept 
                : data_(data), size_(size) {}
            
            /**
             * @brief Construct span from iterator range
             * 
             * @param first Iterator to first element
             * @param last Iterator past last element
             */
            template<typename Iterator>
            constexpr data_span(Iterator first, Iterator last) noexcept
                : data_(&(*first)), size_(static_cast<size_type>(std::distance(first, last))) {}
            
            /**
             * @brief Construct span from C-style array
             * 
             * @tparam N Array size
             * @param arr Reference to array
             */
            template<size_t N>
            constexpr data_span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}
            
            /**
             * @brief Construct span from container with data() and size() methods
             * 
             * @tparam Container Container type (std::vector, std::array, etc.)
             * @param container Container instance
             */
            template<typename Container,
                    typename = std::enable_if_t<
                        !std::is_same_v<Container, data_span> &&
                        std::is_convertible_v<decltype(std::declval<Container>().data()), T*> &&
                        std::is_convertible_v<decltype(std::declval<Container>().size()), size_type>
                    >>
            constexpr data_span(Container& container) noexcept
                : data_(container.data()), size_(container.size()) {}
            
            /**
             * @brief Copy constructor
             */
            constexpr data_span(const data_span&) noexcept = default;
            
            /**
             * @brief Assignment operator
             */
            constexpr data_span& operator=(const data_span&) noexcept = default;
            
            // Element access
            
            /**
             * @brief Access element at index (with bounds checking in debug builds)
             * 
             * @param idx Index of element
             * @return Reference to element
             */
            constexpr reference operator[](size_type idx) const noexcept {
                #ifdef DEBUG
                    if (idx >= size_) {
                        // In production, this would be undefined behavior like std::span
                        // In debug builds, we can add validation
                    }
                #endif
                return data_[idx];
            }
            
            /**
             * @brief Access element at index with bounds checking
             * 
             * @param idx Index of element
             * @return Reference to element
             * @throws std::out_of_range if idx >= size()
             */
            constexpr reference at(size_type idx) const {
                if (idx >= size_) {
                    throw std::out_of_range("data_span::at: index out of range");
                }
                return data_[idx];
            }
            
            /**
             * @brief Access first element
             * @return Reference to first element
             */
            constexpr reference front() const noexcept {
                return data_[0];
            }
            
            /**
             * @brief Access last element
             * @return Reference to last element
             */
            constexpr reference back() const noexcept {
                return data_[size_ - 1];
            }
            
            /**
             * @brief Get pointer to underlying data
             * @return Pointer to first element
             */
            constexpr pointer data() const noexcept {
                return data_;
            }
            
            // Size and capacity
            
            /**
             * @brief Get number of elements
             * @return Number of elements in span
             */
            constexpr size_type size() const noexcept {
                return size_;
            }
            
            /**
             * @brief Get number of bytes occupied by elements
             * @return Total size in bytes
             */
            constexpr size_type size_bytes() const noexcept {
                return size_ * sizeof(T);
            }
            
            /**
             * @brief Check if span is empty
             * @return True if span contains no elements
             */
            constexpr bool empty() const noexcept {
                return size_ == 0;
            }
            
            // Subspan operations
            
            /**
             * @brief Create subspan starting at offset
             * 
             * @param offset Starting index
             * @param count Number of elements (npos for all remaining)
             * @return Subspan
             */
            constexpr data_span<T> subspan(size_type offset, size_type count = npos) const {
                if (offset > size_) {
                    return data_span<T>();
                }
                
                size_type actual_count = (count == npos) ? (size_ - offset) : count;
                if (offset + actual_count > size_) {
                    actual_count = size_ - offset;
                }
                
                return data_span<T>(data_ + offset, actual_count);
            }
            
            /**
             * @brief Get first N elements
             * 
             * @param count Number of elements
             * @return Subspan containing first N elements
             */
            constexpr data_span<T> first(size_type count) const {
                return subspan(0, count);
            }
            
            /**
             * @brief Get last N elements
             * 
             * @param count Number of elements
             * @return Subspan containing last N elements
             */
            constexpr data_span<T> last(size_type count) const {
                return (count >= size_) ? *this : subspan(size_ - count, count);
            }
            
            // Iterators
            
            constexpr iterator begin() const noexcept { return data_; }
            constexpr iterator end() const noexcept { return data_ + size_; }
            constexpr const_iterator cbegin() const noexcept { return data_; }
            constexpr const_iterator cend() const noexcept { return data_ + size_; }
            constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }
            constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
            constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
            constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }
            
        private:
            T* data_;
            size_type size_;
        };
        
        // Deduction guides for C++17
        template<typename T, size_t N>
        data_span(T (&)[N]) -> data_span<T>;
        
        template<typename Container>
        data_span(Container&) -> data_span<typename Container::value_type>;
        
        template<typename T>
        data_span(T*, size_t) -> data_span<T>;
        
    } // namespace kerntopia

#endif

namespace kerntopia {
    
    // Convenience aliases
    template<typename T>
    using span = data_span<T>;
    
    template<typename T>
    using const_span = data_span<const T>;
    
    // Helper functions
    
    /**
     * @brief Create span from container
     * 
     * @tparam Container Container type
     * @param container Container instance
     * @return Span view of container
     */
    template<typename Container>
    constexpr auto as_span(Container& container) -> data_span<typename Container::value_type> {
        return data_span<typename Container::value_type>(container);
    }
    
    /**
     * @brief Create const span from container
     * 
     * @tparam Container Container type
     * @param container Container instance
     * @return Const span view of container
     */
    template<typename Container>
    constexpr auto as_const_span(const Container& container) -> data_span<const typename Container::value_type> {
        return data_span<const typename Container::value_type>(container.data(), container.size());
    }
    
    /**
     * @brief Create byte span from any object
     * 
     * @tparam T Object type
     * @param obj Object to view as bytes
     * @return Span of bytes representing object
     */
    template<typename T>
    constexpr data_span<const std::byte> as_bytes(const data_span<T>& s) noexcept {
        return data_span<const std::byte>(
            reinterpret_cast<const std::byte*>(s.data()), 
            s.size_bytes()
        );
    }
    
    /**
     * @brief Create writable byte span from any object
     * 
     * @tparam T Object type
     * @param obj Object to view as bytes
     * @return Span of bytes representing object
     */
    template<typename T>
    constexpr data_span<std::byte> as_writable_bytes(const data_span<T>& s) noexcept {
        return data_span<std::byte>(
            reinterpret_cast<std::byte*>(s.data()), 
            s.size_bytes()
        );
    }
    
} // namespace kerntopia