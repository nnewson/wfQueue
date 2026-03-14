#ifndef WQ_WAIT_FREE_QUEUE_H
#define WQ_WAIT_FREE_QUEUE_H

#include <atomic>
#include <cassert>
#include <concepts>
#include <new>
#include <type_traits>
#include <utility>

namespace wq
{

// Cache line size for alignment.
// Pre-C++17 doesn't have std::hardware_destructive_interference_size,
// so we use a common default of 64 bytes.
#ifdef __cpp_lib_hardware_interference_size
inline constexpr std::size_t cache_line_size = std::hardware_destructive_interference_size;
#else
inline constexpr std::size_t cache_line_size = 64;
#endif

// MPSC queue node. T must be destructible.
template <typename T>
    requires std::destructible<T>
class WaitFreeNode
{
public:
    WaitFreeNode()
        : m_data(),
          m_next(nullptr),
          m_isSentinel(false)
    {
    }

    template <typename... Args>
        requires std::constructible_from<T, Args...>
    explicit WaitFreeNode(Args&&... args)
        : m_data(std::forward<Args>(args)...),
          m_next(nullptr),
          m_isSentinel(false)
    {
    }

    ~WaitFreeNode() = default;

    WaitFreeNode(const WaitFreeNode&) = delete;
    WaitFreeNode(WaitFreeNode&&) = delete;
    WaitFreeNode& operator=(const WaitFreeNode&) = delete;
    WaitFreeNode& operator=(WaitFreeNode&&) = delete;

    [[nodiscard]]
    bool isSentinel() const noexcept
    {
        return m_isSentinel.load(std::memory_order_relaxed);
    }

    [[nodiscard]]
    const T& data() const noexcept
    {
        return m_data;
    }
    [[nodiscard]]
    T& data() noexcept
    {
        return m_data;
    }

    // Internal — used by WaitFreeQueue. Not intended for direct use.
    void setNext(WaitFreeNode* const node) noexcept
    {
        m_next.store(node, std::memory_order_release);
    }

    [[nodiscard]] WaitFreeNode* next() const noexcept
    {
        return m_next.load(std::memory_order_acquire);
    }

    void setSentinel() noexcept
    {
        m_isSentinel.store(true, std::memory_order_relaxed);
    }

    T m_data;
    std::atomic<WaitFreeNode*> m_next;
    std::atomic<bool> m_isSentinel;
};

// Wait-free MPSC (multiple-producer, single-consumer) queue.
//
// Producers call push() concurrently. A single consumer calls front()/pop().
// Deleter is stored by value to avoid std::function overhead.
template <typename T, typename Deleter = void (*)(WaitFreeNode<T>*)>
    requires std::destructible<T> && std::invocable<Deleter, WaitFreeNode<T>*>
class WaitFreeQueue
{
public:
    explicit WaitFreeQueue(WaitFreeNode<T>* const initialSentinelNode, Deleter deleter)
        : m_head(initialSentinelNode),
          m_tail(initialSentinelNode),
          m_deleter(std::move(deleter))
    {
        initialSentinelNode->setSentinel();
    }

    // Destructor drains any remaining nodes, then destroys the sentinel.
    ~WaitFreeQueue()
    {
        while (pop())
        {
        }
        m_deleter(m_head.load(std::memory_order_relaxed));
    }

    WaitFreeQueue(const WaitFreeQueue&) = delete;
    WaitFreeQueue(WaitFreeQueue&&) = delete;
    WaitFreeQueue& operator=(const WaitFreeQueue&) = delete;
    WaitFreeQueue& operator=(WaitFreeQueue&&) = delete;

    // Push a node. Safe to call from multiple threads concurrently.
    void push(WaitFreeNode<T>* const node) noexcept
    {
        node->setNext(nullptr);
        auto prevNode = m_tail.exchange(node, std::memory_order_acq_rel);
        prevNode->setNext(node);
    }

    // Returns the current head node, or nullptr if empty.
    // Must only be called from the single consumer thread.
    [[nodiscard]] WaitFreeNode<T>* front() noexcept
    {
        auto currentNode = m_head.load(std::memory_order_relaxed);
        auto nextNode = currentNode->next();
        if (!currentNode->isSentinel() || nextNode != nullptr)
        {
            return currentNode;
        }
        // Only the sentinel node remains — queue is empty
        return nullptr;
    }

    // Pops and deletes the head node. Returns true if a node was removed.
    // Must only be called from the single consumer thread.
    bool pop() noexcept
    {
        auto currentNode = m_head.load(std::memory_order_relaxed);
        auto nextNode = currentNode->next();
        if (nextNode != nullptr)
        {
            m_head.store(nextNode, std::memory_order_relaxed);
            m_deleter(currentNode);
            return true;
        }
        // In this case we only have one node - the sentinel node, and we're empty
        currentNode->setSentinel();
        return false;
    }

private:
    alignas(cache_line_size) std::atomic<WaitFreeNode<T>*> m_head;
    alignas(cache_line_size) std::atomic<WaitFreeNode<T>*> m_tail;
    [[no_unique_address]] Deleter m_deleter;
};
} // namespace wq

#endif // WQ_WAIT_FREE_QUEUE_H
