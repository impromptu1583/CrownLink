#include <queue>
#include <mutex>
#include <condition_variable>

struct ThQueueEnded : std::runtime_error {
    ThQueueEnded() : std::runtime_error{"ThQueue has ended!"} {};
};

template <typename T>
class ThQueue {
public:
    ThQueue() = default;
    ThQueue(const ThQueue&) = delete;
    ThQueue& operator=(const ThQueue&) = delete;

    ~ThQueue() {
        end();
    }

    void push(T&& t) {
        std::unique_lock lock{m_mutex};
        m_queue.push(std::forward<T>(t));
        m_cv.notify_one();
    }

    void emplace(auto&&... args) {
        std::unique_lock lock{m_mutex};
        m_queue.emplace(std::forward<decltype(args)>(args)...);
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
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};