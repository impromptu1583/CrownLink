#include <iostream>
#include <thread>
#include "ThQueue.h"

void example1() {
    ThQueue<int> queue;
    std::jthread thread{[&queue]{
        try {
            while (true) {
                std::cout << queue.pop() << "\n";
            }
        } catch (const ThQueueEnded&) {
            // end...
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
        try {
            while (true) {
                std::cout << queue.pop() << "\n";
            }
        } catch (const ThQueueEnded&) {
            // end...
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
        try {
            while (true) {
                auto [priority, value] = queue.pop();
                std::cout << priority << " " << value << "\n";
            }
        } catch (const ThQueueEnded&) {
            // end...
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

    return 0;
}