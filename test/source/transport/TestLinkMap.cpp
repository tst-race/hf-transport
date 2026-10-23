#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "../../../source/common/LinkMap.h"
#include "../../../source/transport/Link.h"
#include "../../../source/transport/LinkAddress.h"

using namespace hf_transport;

class MockLink : public Link {
public:
    MockLink(const LinkID& id) : Link(id, HfLinkAddress(), LinkProperties(), false, nullptr, nullptr) {}
    MOCK_METHOD(std::string, postOnActionThread, (const std::string&, const std::vector<RaceHandle>&, uint64_t), (override));
    MOCK_METHOD(bool, uploadContent, (const std::vector<uint8_t>&, const std::string&, std::string&), (override));
    MOCK_METHOD(std::string, fetchOnActionThread, (const std::string&), (override));
    MOCK_METHOD(bool, downloadNewCommits, (const std::string&, std::vector<std::pair<std::string, std::vector<uint8_t>>>&), (override));
};

TEST(LinkMapTest, PutAndGet) {
    LinkMap map;
    auto link = std::make_shared<MockLink>("link-1");
    
    map.put("link-1", link);
    EXPECT_TRUE(map.contains("link-1"));
    EXPECT_EQ(map.get("link-1"), link);
    EXPECT_EQ(map.size(), 1);
}

TEST(LinkMapTest, GetNonExistent) {
    LinkMap map;
    EXPECT_FALSE(map.contains("link-999"));
    EXPECT_EQ(map.get("link-999"), nullptr);
}

TEST(LinkMapTest, Remove) {
    LinkMap map;
    auto link = std::make_shared<MockLink>("link-1");
    
    map.put("link-1", link);
    map.remove("link-1");
    EXPECT_FALSE(map.contains("link-1"));
    EXPECT_EQ(map.size(), 0);
}

TEST(LinkMapTest, GetAllIds) {
    LinkMap map;
    map.put("link-1", std::make_shared<MockLink>("link-1"));
    map.put("link-2", std::make_shared<MockLink>("link-2"));
    map.put("link-3", std::make_shared<MockLink>("link-3"));
    
    auto ids = map.getAllIds();
    EXPECT_EQ(ids.size(), 3);
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "link-1") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "link-2") != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), "link-3") != ids.end());
}

TEST(LinkMapTest, ThreadSafety) {
    LinkMap map;
    const int numThreads = 10;
    const int opsPerThread = 100;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&map, i, opsPerThread]() {
            (void)opsPerThread; // Suppress unused capture warning
            for (int j = 0; j < opsPerThread; ++j) {
                std::string id = "link-" + std::to_string(i * opsPerThread + j);
                auto link = std::make_shared<MockLink>(id);
                map.put(id, link);
                EXPECT_NE(map.get(id), nullptr);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(map.size(), numThreads * opsPerThread);
}