#ifndef WQ_WAIT_FREE_QUEUE_H
#define WQ_WAIT_FREE_QUEUE_H

#include <atomic>
#include <memory>
#include <functional>

namespace wq {
    // Declarations
    template<typename T>
    class WaitFreeQueue;

    // Public Interface Classes
    template<typename T>
    class WaitFreeNode {
    public:
        WaitFreeNode()
            : m_data()
            , m_next(nullptr)
            , m_isSentinel(false)
        {
        }

        template<typename... Args>
        WaitFreeNode(Args... args)
            : m_data(std::forward<Args>(args)...)
            , m_next(nullptr)
            , m_isSentinel(false)
        {
        }

        ~WaitFreeNode() = default;

        WaitFreeNode(const WaitFreeNode &) = delete;
        WaitFreeNode(WaitFreeNode &&) = delete;
        WaitFreeNode &operator=(const WaitFreeNode &) = delete;
        WaitFreeNode &operator=(WaitFreeNode &&) = delete;

        bool isSentinel() const noexcept {
            return m_isSentinel;
        }

        T                               m_data;

    private:
        friend class WaitFreeQueue<T>;

        void setNext(WaitFreeNode *const node) noexcept {
            m_next.store(node, std::memory_order_release);
        }

        WaitFreeNode* next() const noexcept {
            return m_next.load(std::memory_order_acquire);
        }

        void setSentinel() noexcept {
            m_isSentinel = true;
        }

        std::atomic<WaitFreeNode<T> *>  m_next;
        bool                            m_isSentinel;
    };

    template<typename T>
    class WaitFreeQueue {
    public:
        template<typename Deleter >
        explicit WaitFreeQueue(WaitFreeNode<T>* const initialSentinelNode,
                               Deleter deleter )
            : m_head(initialSentinelNode)
            , m_deleter(std::move(deleter))
            , m_tail(initialSentinelNode)
        {
            initialSentinelNode->setSentinel();
        }

        // Current the destructor assumes the queue is empty
        // i.e. the front returns nullptr, thus it destroys only the sentinel node
        ~WaitFreeQueue() {
            m_deleter(m_head);
        };

        WaitFreeQueue(const WaitFreeQueue &) = delete;
        WaitFreeQueue(WaitFreeQueue &&) = delete;
        WaitFreeQueue &operator=(const WaitFreeQueue &) = delete;
        WaitFreeQueue &operator=(WaitFreeQueue &&) = delete;

        void push(WaitFreeNode<T>* const node) noexcept {
            node->setNext(nullptr);
            auto prevNode = m_tail.exchange(node, std::memory_order_acq_rel);
            prevNode->setNext(node);
        }

        // Returns the current head node for processing and clearing out
        // The return value should not be deleted.
        WaitFreeNode<T>* front() noexcept {
            auto currentNode = m_head;
            auto nextNode = currentNode->next();
            if ( !currentNode->isSentinel() || nextNode != nullptr ) {
                return currentNode;
            }
            else {
                // In this case we only have one node - the sentinel node, and we're empty
                return nullptr;
            }
        }

        // Pops the destructed head node and returns it for deletion
        bool pop() noexcept {
            auto currentNode = m_head;
            auto nextNode = currentNode->next();
            if ( nextNode != nullptr ) {
                m_head = nextNode;
                m_deleter(currentNode);
                return true;
            }
            else {
                // In this case we only have one node - the sentinel node, and we're empty
                currentNode->setSentinel();
                return false;
            }
        }

    private:
        WaitFreeNode<T>*                            m_head;
        std::function<void(WaitFreeNode<T>*)>       m_deleter;
        alignas(64) std::atomic<WaitFreeNode<T> *>  m_tail;
    };
}

#endif //WQ_WAIT_FREE_QUEUE_H
