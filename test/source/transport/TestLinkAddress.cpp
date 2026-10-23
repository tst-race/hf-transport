#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "../../../source/transport/LinkAddress.h"

using namespace hf_transport;

TEST(LinkAddressTest, DefaultConstruction) {
    HfLinkAddress addr;
    EXPECT_EQ(addr.username, "");
    EXPECT_EQ(addr.repoId, "");
    EXPECT_EQ(addr.initialCommitSha, "");
    EXPECT_EQ(addr.branch, "main");
    EXPECT_EQ(addr.maxTries, 120);
    EXPECT_EQ(addr.openObjects, 1);
    EXPECT_EQ(addr.singleReceive, false);
    EXPECT_FALSE(addr.isValid());
}

TEST(LinkAddressTest, ConstructionWithRequiredFields) {
    HfLinkAddress addr("testuser", "transport-abc123", "commit123", "main");
    EXPECT_EQ(addr.username, "testuser");
    EXPECT_EQ(addr.repoId, "transport-abc123");
    EXPECT_EQ(addr.initialCommitSha, "commit123");
    EXPECT_EQ(addr.branch, "main");
    EXPECT_TRUE(addr.isValid());
}

TEST(LinkAddressTest, ToString) {
    HfLinkAddress addr("testuser", "transport-abc123", "commit123", "main");
    std::string str = addr.toString();
    EXPECT_EQ(str, "hf://testuser/transport-abc123@commit123");
}

TEST(LinkAddressTest, ToStringWithBranch) {
    HfLinkAddress addr("testuser", "transport-abc123", "commit123", "develop");
    std::string str = addr.toString();
    EXPECT_EQ(str, "hf://testuser/transport-abc123@commit123?branch=develop");
}

TEST(LinkAddressTest, FromStringBasic) {
    HfLinkAddress addr = HfLinkAddress::fromString("hf://testuser/transport-abc123@commit123");
    EXPECT_EQ(addr.username, "testuser");
    EXPECT_EQ(addr.repoId, "transport-abc123");
    EXPECT_EQ(addr.initialCommitSha, "commit123");
    EXPECT_EQ(addr.branch, "main");
}

TEST(LinkAddressTest, FromStringWithBranch) {
    HfLinkAddress addr = HfLinkAddress::fromString("hf://testuser/transport-abc123@commit123?branch=develop");
    EXPECT_EQ(addr.username, "testuser");
    EXPECT_EQ(addr.repoId, "transport-abc123");
    EXPECT_EQ(addr.initialCommitSha, "commit123");
    EXPECT_EQ(addr.branch, "develop");
}

TEST(LinkAddressTest, RoundTrip) {
    HfLinkAddress original("testuser", "transport-abc123", "commit123", "develop");
    std::string str = original.toString();
    HfLinkAddress parsed = HfLinkAddress::fromString(str);
    
    EXPECT_EQ(parsed.username, original.username);
    EXPECT_EQ(parsed.repoId, original.repoId);
    EXPECT_EQ(parsed.initialCommitSha, original.initialCommitSha);
    EXPECT_EQ(parsed.branch, original.branch);
}

TEST(LinkAddressTest, JsonSerialization) {
    HfLinkAddress addr("testuser", "transport-abc123", "commit123", "main");
    addr.maxTries = 60;
    addr.openObjects = 2;
    addr.singleReceive = true;
    
    nlohmann::json j = addr;
    
    EXPECT_EQ(j["username"], "testuser");
    EXPECT_EQ(j["repoId"], "transport-abc123");
    EXPECT_EQ(j["initialCommitSha"], "commit123");
    EXPECT_EQ(j["branch"], "main");
    EXPECT_EQ(j["maxTries"], 60);
    EXPECT_EQ(j["openObjects"], 2);
    EXPECT_EQ(j["singleReceive"], true);
}

TEST(LinkAddressTest, JsonDeserialization) {
    nlohmann::json j = {
        {"username", "testuser"},
        {"repoId", "transport-abc123"},
        {"initialCommitSha", "commit123"},
        {"branch", "develop"},
        {"maxTries", 60},
        {"openObjects", 2},
        {"singleReceive", true}
    };
    
    HfLinkAddress addr = j.get<HfLinkAddress>();
    
    EXPECT_EQ(addr.username, "testuser");
    EXPECT_EQ(addr.repoId, "transport-abc123");
    EXPECT_EQ(addr.initialCommitSha, "commit123");
    EXPECT_EQ(addr.branch, "develop");
    EXPECT_EQ(addr.maxTries, 60);
    EXPECT_EQ(addr.openObjects, 2);
    EXPECT_EQ(addr.singleReceive, true);
}

TEST(LinkAddressTest, JsonRoundTrip) {
    HfLinkAddress original("testuser", "transport-abc123", "commit123", "main");
    nlohmann::json j = original;
    HfLinkAddress parsed = j.get<HfLinkAddress>();
    
    EXPECT_EQ(parsed.username, original.username);
    EXPECT_EQ(parsed.repoId, original.repoId);
    EXPECT_EQ(parsed.initialCommitSha, original.initialCommitSha);
    EXPECT_EQ(parsed.branch, original.branch);
    EXPECT_EQ(parsed.maxTries, original.maxTries);
    EXPECT_EQ(parsed.openObjects, original.openObjects);
    EXPECT_EQ(parsed.singleReceive, original.singleReceive);
}

TEST(LinkAddressTest, FromStringInvalidFormat) {
    HfLinkAddress addr = HfLinkAddress::fromString("invalid://format");
    EXPECT_FALSE(addr.isValid());
}

TEST(LinkAddressTest, FromStringEmpty) {
    HfLinkAddress addr = HfLinkAddress::fromString("");
    EXPECT_FALSE(addr.isValid());
}