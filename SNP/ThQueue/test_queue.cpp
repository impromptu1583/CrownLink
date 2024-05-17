#include <iostream>
#include <thread>
#include "ThQueue.h"

void example1() {
    ThQueue<int> queue;
    std::jthread thread{[&queue]{
        while (const auto value = queue.pop()) {
            std::cout << *value << "\n";
        }
    }};

    queue.push(1);
    queue.push(2);
    queue.push(3);
    queue.end();
}

void example2() {
    ThQueue<std::string> queue;
    std::jthread thread{[&queue]{
        std::string str;
        while (queue.try_pop(str)) {
            std::cout << str << "\n";
        }
    }};

    queue.emplace("Hello");
    queue.emplace("World");
    queue.end();
}

enum class Priority { Low, Medium, High };

std::ostream& operator<<(std::ostream& out, Priority priority) {
    switch (priority) {
        case Priority::Low:    return out << "Priority::Low";
        case Priority::Medium: return out << "Priority::Medium";
        case Priority::High:   return out << "Priority::High";
    }
    return out << (int)priority;
}

void example3() {
    ThPriorityQueue<Priority, int> queue;
    std::jthread thread{[&queue]{
        while (const auto pair = queue.pop()) {
            auto [priority, value] = *pair;
            std::cout << priority << " " << value << "\n";
        }
    }};

    queue.emplace(Priority::Low, 123);
    queue.emplace(Priority::Low, 123);
    queue.emplace(Priority::Low, 123);
    queue.emplace(Priority::Low, 123);
    queue.emplace(Priority::Medium, 321);
    queue.emplace(Priority::High, 432);
    queue.push({Priority::Low, 123});
    queue.end();
}

void example4() {
    bool game_loop_is_running = true;

    ThQueue<std::string> queue;
    std::jthread thread{[&queue, &game_loop_is_running]{
        while (game_loop_is_running) {
            std::cout << "Game Tick!\n";
            while (const auto str = queue.pop_dont_wait()) {
                std::cout << *str << "\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }};

    queue.emplace("Hello");
    queue.emplace("World");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    game_loop_is_running = false;
}

#define RUN(func) do {\
    std::cout << "  ==========\n"; \
    std::cout << "   " << #func << "\n"; \
    std::cout << "  ==========\n"; \
    func(); \
    std::cout << "\n"; \
} while (false)

int main(int argc, char** argv) {
    RUN(example1);
    RUN(example2);
    RUN(example3);
    RUN(example4);

    return 0;
}