#include <queue>
#include <mutex>
#include <condition_variable>

struct ThQueueEnded : std::runtime_error {
    ThQueueEnded() : std::runtime_error{"ThQueue has ended!"} {};
};

template <typename T, typename Queue = std::queue<T>>
class ThQueue {
public:
    ThQueue() = default;
    ThQueue(const ThQueue&) = delete;
    ThQueue& operator=(const ThQueue&) = delete;

    ~ThQueue() {
        end();
    }

    void push(T&& value) {
        std::unique_lock lock{m_mutex};
        m_queue.push(std::forward<T>(value));
        m_cv.notify_one();
    }

    template <typename... Ts>
    void emplace(Ts&&... args) {
        std::unique_lock lock{m_mutex};
        m_queue.emplace(std::forward<Ts>(args)...);
        m_cv.notify_one();
    }

    T pop() {
        std::unique_lock lock{m_mutex};
        m_cv.wait(lock, [this]{ return m_ended || !m_queue.empty(); });

        if (m_ended && m_queue.empty()) {
            // Not sure I love this, but not sure how else to end the wait,
            // without having to return like std::optional or std::expected
            throw ThQueueEnded{};
        }
        
        T result = std::move(m_queue.front());
        m_queue.pop();
        return result;
    }

    void end() {
        std::unique_lock lock{m_mutex};
        m_ended = true;
        m_cv.notify_all();
    }

private:
    bool m_ended{false};
    Queue m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

template <typename Pair>
class PriorityQueueWrapper {
public:
    void push(Pair&& value) {
        m_queue.push(std::forward<Pair>(value));
    }

    template <typename... Ts>
    void emplace(Ts&&... args) {
        m_queue.emplace(std::forward<Ts>(args)...);
    }

    Pair front() {
        return std::move(m_queue.top());
    }

    void pop() {
        m_queue.pop();
    }

    bool empty() const {
        return m_queue.empty();
    }

private:
    struct Comparator {
        constexpr bool operator()(const Pair& a, const Pair& b) const {
            return a.first < b.first;
        }
    };
    std::priority_queue<Pair, std::vector<Pair>, Comparator> m_queue;
};

template <typename Prio, typename T>
using ThPriorityQueue = ThQueue<std::pair<Prio, T>, PriorityQueueWrapper<std::pair<Prio, T>>>;