#include <gtest/gtest.h>

#include <wfQueue.h>

// Constants
constexpr size_t c_freeListSize = 10000000;

// Types
struct TestNode
{
    TestNode(unsigned val1, unsigned val2)
        : m_val1(val1)
        , m_val2(val2)
    {
    }

    TestNode()
        : m_val1(0)
        , m_val2(0)
    {
    }

    unsigned    m_val1;
    unsigned    m_val2;
};

// Tests
TEST(FreeListTest, testSingleConsumerSingleProducer)
{
    wq::WaitFreeNode< TestNode >*   sentinelNode = new wq::WaitFreeNode< TestNode >();

    wq::WaitFreeQueue< TestNode >   queue( sentinelNode, [](auto node){ delete node; } );
    ASSERT_EQ( queue.front(), nullptr );
    ASSERT_FALSE( queue.pop() );

    queue.push( new wq::WaitFreeNode< TestNode > ( 1, 2 ) );
    auto node = queue.front();
    ASSERT_EQ( node, sentinelNode );
    ASSERT_TRUE( node->isSentinel() );
    ASSERT_TRUE( queue.pop() );

    node = queue.front();
    ASSERT_EQ( node->m_data.m_val1, 1 );
    ASSERT_EQ( node->m_data.m_val2, 2 );

    ASSERT_FALSE( queue.pop() );
}

/*
template< typename T >
void allocatorTestThread(std::shared_ptr< T > freeList)
{
    std::vector< typename T::ptr > nodes(c_freeListSize);

    for (size_t i = 0 ; i < c_freeListSize ; ++i) {
        auto node = freeList->construct(i, i);

        if (node) {
            nodes[i] = std::move(node);
        }
        else {
            break;
        }
    }

    for (size_t i = 0 ; i < c_freeListSize ; ++i) {
        if (nodes[i] != nullptr) {
            nodes[i] = nullptr;
        }
        else {
            break;
        }
    }
}

template< typename T >
void testMultithreaded(std::shared_ptr< T > freeList)
{
    size_t numThreads = 4;
    std::future< void > fut[numThreads];

    for (size_t i = 0 ; i < numThreads ; ++i) {
        fut[i] = std::async(std::launch::async, allocatorTestThread< T >, freeList);
    }

    for (size_t i = 0 ; i < numThreads ; ++i) {
        fut[i].wait();
    }

    // If we get here the code didn't lock up
    ASSERT_TRUE(true);
}

TEST(FreeListTest, testMultithreadedStaticMTMT)
{
    auto freeList = std::make_shared< fl::FreeListStaticMultipleProducerMultipleConsumer< TestNode, c_freeListSize > >();
    testMultithreaded(freeList);
}

TEST(FreeListTest, testMultithreadedDynamicMTMT)
{
    auto freeList = std::make_shared< fl::FreeListDynamicMultipleProducerMultipleConsumer< TestNode > >(c_freeListSize);
    testMultithreaded(freeList);
}*/