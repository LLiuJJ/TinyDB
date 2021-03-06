/**
 * @file DataQueueTests.cpp
 *
 * This module contains the unit tests of the
 * SystemAbstractions::DataQueue class.
 *
 * © 2020 by LiuJ
 */

#include <DataQueue.hpp>
#include <gtest/gtest.h>
#include <stdint.h>
#include <vector>

TEST(DataQueueTests, EnqueueCopy) {
    
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10000000, 'X');

    q.Enqueue(data);
    data.assign(5000, 'Y');
    q.Enqueue(data);

    EXPECT_EQ(2, q.GetBuffersQueued());
    EXPECT_EQ(10005000, q.GetBytesQueued());
}

TEST(DataQueueTests, EnqueueMove) {

    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10000000, 'X');

    q.Enqueue(std::move(data));
    data.assign(5000, 'Y');
    q.Enqueue(std::move(data));

    EXPECT_EQ(2, q.GetBuffersQueued());
    EXPECT_EQ(10005000, q.GetBytesQueued());
}

TEST(DataQueueTests, DequeuePartialBuffer) {
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    data = q.Dequeue(8);

    EXPECT_EQ(
        "XXXXXXXX",
        std::string(data.begin(), data.end())
    );
    EXPECT_EQ(2, q.GetBuffersQueued());
    EXPECT_EQ(7, q.GetBytesQueued());
}

TEST(DataQueueTests, DequeueExacltyOneFullBuffer) {

    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    data = q.Dequeue(10);

    EXPECT_EQ(
        "XXXXXXXXXX",
        std::string(data.begin(), data.end())
    );
    EXPECT_EQ(1, q.GetBuffersQueued());
    EXPECT_EQ(5, q.GetBytesQueued());
}


TEST(DataQueueTests, DequeueFullBufferPlusPartialNextBuffer) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    // Act
    data = q.Dequeue(12);

    // Assert
    EXPECT_EQ(
        "XXXXXXXXXXYY",
        std::string(data.begin(), data.end())
    );
    EXPECT_EQ(1, q.GetBuffersQueued());
    EXPECT_EQ(3, q.GetBytesQueued());
}

TEST(DataQueueTests, DequeueExactlyOneFullBufferThenTheRest) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);
    data = q.Dequeue(10);

    // Act
    data = q.Dequeue(5);

    // Assert
    EXPECT_EQ(
        "YYYYY",
        std::string(data.begin(), data.end())
    );
    EXPECT_EQ(0, q.GetBuffersQueued());
    EXPECT_EQ(0, q.GetBytesQueued());
}

TEST(DataQueueTests, DequeueFullBufferPlusPartialNextBufferThenTheRest) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);
    data = q.Dequeue(12);

    // Act
    data = q.Dequeue(3);

    // Assert
    EXPECT_EQ(
        "YYY",
        std::string(data.begin(), data.end())
    );
    EXPECT_EQ(0, q.GetBuffersQueued());
    EXPECT_EQ(0, q.GetBytesQueued());
}

TEST(DataQueueTests, PeekPartialBuffer) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    // Act
    data = q.Peek(8);

    // Assert
    EXPECT_EQ(
        "XXXXXXXX",
        std::string(data.begin(), data.end())
    );
    EXPECT_EQ(2, q.GetBuffersQueued());
    EXPECT_EQ(15, q.GetBytesQueued());
}

TEST(DataQueueTests, PeekExactlyOneFullBuffer) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    // Act
    data = q.Peek(10);

    // Assert
    EXPECT_EQ(
        "XXXXXXXXXX",
        std::string(data.begin(), data.end())
    );
    EXPECT_EQ(2, q.GetBuffersQueued());
    EXPECT_EQ(15, q.GetBytesQueued());
}

TEST(DataQueueTests, PeekFullBufferPlusPartialNextBuffer) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    // Act
    data = q.Peek(12);

    // Assert
    EXPECT_EQ(
        "XXXXXXXXXXYY",
        std::string(data.begin(), data.end())
    );
    EXPECT_EQ(2, q.GetBuffersQueued());
    EXPECT_EQ(15, q.GetBytesQueued());
}

TEST(DataQueueTests, DropPartialBuffer) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    // Act
    q.Drop(8);

    // Assert
    EXPECT_EQ(2, q.GetBuffersQueued());
    EXPECT_EQ(7, q.GetBytesQueued());
}

TEST(DataQueueTests, DropExactlyOneFullBuffer) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    // Act
    q.Drop(10);

    // Assert
    EXPECT_EQ(1, q.GetBuffersQueued());
    EXPECT_EQ(5, q.GetBytesQueued());
}

TEST(DataQueueTests, DropFullBufferPlusPartialNextBuffer) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    data.assign(5, 'Y');
    q.Enqueue(data);

    // Act
    q.Drop(12);

    // Assert
    EXPECT_EQ(1, q.GetBuffersQueued());
    EXPECT_EQ(3, q.GetBytesQueued());
}

// This recreates a specific bug in Dequeue, where a local variable
// tracking the number of remaining bytes in the queue was subtracted
// by the wrong value, causing the actual number of bytes returned
// and/or removed to be lower than expected.
TEST(DataQueueTests, ShortenedDequeueBug) {
    // Arrange
    SystemAbstractions::DataQueue q;
    std::vector< uint8_t > data(10, 'X');
    q.Enqueue(data);
    q.Enqueue(data);
    q.Enqueue(data);

    // Act
    const auto buffer = q.Peek(30);

    // Assert
    EXPECT_EQ(30, buffer.size());
}
