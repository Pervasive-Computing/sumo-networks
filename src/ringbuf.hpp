#pragma once

#include <array>
#include <optional>
#include <iostream>
// #include <vector>

// template <typename T, std::size_t N, class Container = std::array<T, N>>
template <typename T, std::size_t N>
class RingBuffer {
  public:
	auto push_back(const T& item) -> void {
        this->buffer[this->tail] = item;
        this->tail = (this->tail + 1) % N;
        if (this->num_items == N) {
            this->head = (this->head + 1) % N;
        } else {
            this->num_items++;
        }
    }
	auto pop_front() -> bool {
        if (!this->empty()) {
            this->head = (this->head + 1) % N;
            this->num_items--;
            return true;
        }
        return false;
    }
    auto pop_back() -> bool {
        if (!this->empty()) {
            this->tail = (this->tail - 1) % N;
            this->num_items--;
            return true;
        }
        return false;
    }

    auto back() const -> std::optional<T> { return !this->empty() ? std::optional<T>{this->buffer[this->tail - 1]} : std::nullopt; }
	auto front() const -> std::optional<T> { return !this->empty() ? std::optional<T>{this->buffer[this->head]} : std::nullopt; }
    auto full() const -> bool { return this->num_items == N; }
    auto empty() const -> bool { return this->num_items == 0; }
    auto size() const -> std::size_t { return this->num_items; }
    auto capacity() const -> std::size_t { return N; }
    auto underlying() const -> const std::array<T, N>& { return this->buffer; }


    auto print_internal_state() const -> void {
        std::cout << "head: " << this->head << std::endl;
        std::cout << "tail: " << this->tail << std::endl;
        std::cout << "num_items: " << this->num_items << std::endl;
        std::cout << "buffer: ";
        for (auto& item : this->buffer) {
            std::cout << item << " ";
        }
        std::cout << std::endl;
    }

  private:
	std::array<T, N> buffer;
	std::size_t		 head = 0;
	std::size_t		 tail = 0;
	std::size_t		 num_items = 0;
};
