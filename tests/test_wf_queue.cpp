#include <gtest/gtest.h>

#include <wf_queue.h>

#include <atomic>
#include <barrier>
#include <latch>
#include <string>
#include <thread>
#include <vector>

// --- Test types ---

struct TestNode
{
    unsigned m_val1;
    unsigned m_val2;
};

// Helper to create a queue with a lambda deleter
auto makeQueue(wq::WaitFreeNode<TestNode>* sentinel)
{
    auto deleter = [](wq::WaitFreeNode<TestNode>* node) { delete node; };
    return wq::WaitFreeQueue<TestNode, decltype(deleter)>(sentinel, deleter);
}

// --- Node tests ---

TEST(WaitFreeNode, DefaultConstruction)
{
    wq::WaitFreeNode<TestNode> node;
    EXPECT_EQ(node.data().m_val1, 0u);
    EXPECT_EQ(node.data().m_val2, 0u);
    EXPECT_FALSE(node.isSentinel());
    EXPECT_EQ(node.next(), nullptr);
}

TEST(WaitFreeNode, ForwardingConstruction)
{
    wq::WaitFreeNode<TestNode> node(42u, 99u);
    EXPECT_EQ(node.data().m_val1, 42u);
    EXPECT_EQ(node.data().m_val2, 99u);
    EXPECT_FALSE(node.isSentinel());
}

TEST(WaitFreeNode, SentinelFlag)
{
    wq::WaitFreeNode<TestNode> node;
    EXPECT_FALSE(node.isSentinel());
    node.setSentinel();
    EXPECT_TRUE(node.isSentinel());
}

TEST(WaitFreeNode, NextLinkage)
{
    wq::WaitFreeNode<TestNode> a;
    wq::WaitFreeNode<TestNode> b;
    EXPECT_EQ(a.next(), nullptr);
    a.setNext(&b);
    EXPECT_EQ(a.next(), &b);
}

TEST(WaitFreeNode, DataAccessor)
{
    wq::WaitFreeNode<TestNode> node(1u, 2u);

    // Mutable access
    node.data().m_val1 = 10u;
    EXPECT_EQ(node.data().m_val1, 10u);

    // Const access
    const auto& cref = node;
    EXPECT_EQ(cref.data().m_val1, 10u);
    EXPECT_EQ(cref.data().m_val2, 2u);
}

TEST(WaitFreeNode, StringType)
{
    using namespace std::string_literals;
    wq::WaitFreeNode<std::string> node("hello"s);
    EXPECT_EQ(node.data(), "hello");
}

// --- Queue tests ---

TEST(WaitFreeQueue, EmptyQueueReturnsNullptr)
{
    auto queue = makeQueue(new wq::WaitFreeNode<TestNode>());
    EXPECT_EQ(queue.front(), nullptr);
    EXPECT_FALSE(queue.pop());
}

TEST(WaitFreeQueue, PushSingleElement)
{
    auto* sentinel = new wq::WaitFreeNode<TestNode>();
    auto queue = makeQueue(sentinel);

    queue.push(new wq::WaitFreeNode<TestNode>(1u, 2u));

    auto* head = queue.front();
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(head, sentinel);
    EXPECT_TRUE(head->isSentinel());

    ASSERT_TRUE(queue.pop());

    head = queue.front();
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(head->data().m_val1, 1u);
    EXPECT_EQ(head->data().m_val2, 2u);
    EXPECT_FALSE(head->isSentinel());

    EXPECT_FALSE(queue.pop());
}

TEST(WaitFreeQueue, PushMultipleElements)
{
    auto queue = makeQueue(new wq::WaitFreeNode<TestNode>());

    constexpr unsigned count = 10;
    for (unsigned i = 0; i < count; ++i)
    {
        queue.push(new wq::WaitFreeNode<TestNode>(i, i * 10));
    }

    // Pop sentinel
    ASSERT_TRUE(queue.pop());

    // Verify FIFO order
    for (unsigned i = 0; i < count; ++i)
    {
        auto* head = queue.front();
        ASSERT_NE(head, nullptr) << "Expected element at index " << i;
        EXPECT_EQ(head->data().m_val1, i);
        EXPECT_EQ(head->data().m_val2, i * 10);

        if (i < count - 1)
        {
            ASSERT_TRUE(queue.pop());
        }
        else
        {
            // Last element — becomes new sentinel-like tail
            EXPECT_FALSE(queue.pop());
        }
    }
}

TEST(WaitFreeQueue, PopEmptyMultipleTimes)
{
    auto queue = makeQueue(new wq::WaitFreeNode<TestNode>());

    EXPECT_FALSE(queue.pop());
    EXPECT_FALSE(queue.pop());
    EXPECT_FALSE(queue.pop());
    EXPECT_EQ(queue.front(), nullptr);
}

TEST(WaitFreeQueue, PushPopInterleavedSingleThread)
{
    auto queue = makeQueue(new wq::WaitFreeNode<TestNode>());

    // Push one, consume it
    queue.push(new wq::WaitFreeNode<TestNode>(1u, 1u));
    ASSERT_TRUE(queue.pop());  // pops sentinel
    auto* node = queue.front();
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->data().m_val1, 1u);
    EXPECT_FALSE(queue.pop());  // single element, no next — can't pop

    // Push another — the previous node now has a next, so it can be popped
    queue.push(new wq::WaitFreeNode<TestNode>(2u, 2u));
    ASSERT_TRUE(queue.pop());  // pops the node with val1=1
    node = queue.front();
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->data().m_val1, 2u);
    EXPECT_FALSE(queue.pop());  // single element again
}

TEST(WaitFreeQueue, DestructorDrainsRemainingNodes)
{
    std::atomic<int> deleteCount{0};
    auto deleter = [&deleteCount](wq::WaitFreeNode<TestNode>* node)
    {
        deleteCount.fetch_add(1, std::memory_order_relaxed);
        delete node;
    };

    {
        auto* sentinel = new wq::WaitFreeNode<TestNode>();
        wq::WaitFreeQueue<TestNode, decltype(deleter)> queue(sentinel, deleter);

        queue.push(new wq::WaitFreeNode<TestNode>(1u, 1u));
        queue.push(new wq::WaitFreeNode<TestNode>(2u, 2u));
        queue.push(new wq::WaitFreeNode<TestNode>(3u, 3u));
        // Destructor should clean up all 4 nodes (sentinel + 3 pushed)
    }

    EXPECT_EQ(deleteCount.load(), 4);
}

// --- Concurrency tests ---

TEST(WaitFreeQueue, MultipleProducersSingleConsumer)
{
    constexpr unsigned numProducers = 4;
    constexpr unsigned itemsPerProducer = 10'000;

    auto* sentinel = new wq::WaitFreeNode<TestNode>();
    auto deleter = [](wq::WaitFreeNode<TestNode>* node) { delete node; };
    wq::WaitFreeQueue<TestNode, decltype(deleter)> queue(sentinel, deleter);

    std::latch startLatch(numProducers + 1);  // +1 for consumer
    std::atomic<bool> done{false};
    std::atomic<unsigned> totalProduced{0};

    // Producer threads
    std::vector<std::jthread> producers;
    producers.reserve(numProducers);
    for (unsigned p = 0; p < numProducers; ++p)
    {
        producers.emplace_back(
            [&queue, &startLatch, &totalProduced, p]()
            {
                startLatch.arrive_and_wait();
                for (unsigned i = 0; i < itemsPerProducer; ++i)
                {
                    queue.push(new wq::WaitFreeNode<TestNode>(p, i));
                    totalProduced.fetch_add(1, std::memory_order_relaxed);
                }
            });
    }

    // Consumer thread
    unsigned totalConsumed = 0;
    std::jthread consumer(
        [&queue, &startLatch, &done, &totalConsumed]()
        {
            startLatch.arrive_and_wait();
            while (!done.load(std::memory_order_acquire) || queue.front() != nullptr)
            {
                if (queue.front() != nullptr)
                {
                    if (queue.pop())
                    {
                        ++totalConsumed;
                    }
                }
                else
                {
                    std::this_thread::yield();
                }
            }
        });

    // Wait for producers to finish
    for (auto& t : producers)
    {
        t.join();
    }

    done.store(true, std::memory_order_release);
    consumer.join();

    // The sentinel is popped as part of consumption, so totalConsumed includes it.
    // We expect sentinel + all produced items to have been consumed or drained.
    // Drain any remaining in the queue.
    unsigned drained = 0;
    while (queue.front() != nullptr)
    {
        queue.pop();
        ++drained;
    }

    EXPECT_EQ(totalProduced.load(), numProducers * itemsPerProducer);
    EXPECT_EQ(totalConsumed + drained, numProducers * itemsPerProducer);
}

TEST(WaitFreeQueue, ConcurrentPushStressTest)
{
    constexpr unsigned numThreads = 8;
    constexpr unsigned itemsPerThread = 5'000;

    auto* sentinel = new wq::WaitFreeNode<TestNode>();
    auto deleter = [](wq::WaitFreeNode<TestNode>* node) { delete node; };
    wq::WaitFreeQueue<TestNode, decltype(deleter)> queue(sentinel, deleter);

    std::latch startLatch(numThreads);

    std::vector<std::jthread> threads;
    threads.reserve(numThreads);
    for (unsigned t = 0; t < numThreads; ++t)
    {
        threads.emplace_back(
            [&queue, &startLatch, t]()
            {
                startLatch.arrive_and_wait();
                for (unsigned i = 0; i < itemsPerThread; ++i)
                {
                    queue.push(new wq::WaitFreeNode<TestNode>(t, i));
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // Drain and count — should have sentinel + all pushed items
    unsigned count = 0;
    while (queue.front() != nullptr)
    {
        if (queue.pop())
        {
            ++count;
        }
    }

    // We pop the sentinel + all data nodes except the last (destructor handles it).
    // Total successful pops = 1 sentinel + (N - 1) data nodes = N.
    EXPECT_EQ(count, numThreads * itemsPerThread);
}

// --- Type trait / concept tests ---

TEST(WaitFreeNode, WorksWithMoveOnlyType)
{
    wq::WaitFreeNode<std::unique_ptr<int>> node(std::make_unique<int>(42));
    EXPECT_EQ(*node.data(), 42);
}

TEST(WaitFreeNode, WorksWithAggregateViaForwarding)
{
    wq::WaitFreeNode<std::string> node("constructed in place");
    EXPECT_EQ(node.data(), "constructed in place");
}
