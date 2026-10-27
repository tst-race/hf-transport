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

// toString() is the JSON form the framework exchanges as a link address.
TEST(LinkAddressTest, ToStringIsJson) {
    HfLinkAddress addr("testuser", "transport-abc123", "commit123", "develop");
    nlohmann::json j = nlohmann::json::parse(addr.toString());
    EXPECT_EQ(j["type"], "hf");
    EXPECT_EQ(j["username"], "testuser");
    EXPECT_EQ(j["repoId"], "transport-abc123");
    EXPECT_EQ(j["commitSha"], "commit123");
    EXPECT_EQ(j["branch"], "develop");
}

TEST(LinkAddressTest, ParseAcceptsTheAdapterJsonFormat) {
    HfLinkAddress addr = HfLinkAddress::parse(
        R"({"type": "hf", "username": "u", "repoId": "r", "commitSha": "", "branch": "main"})");
    EXPECT_EQ(addr.username, "u");
    EXPECT_EQ(addr.repoId, "r");
    EXPECT_EQ(addr.initialCommitSha, "");
    // An empty commit is legal for a loader (it starts from the first commit) but not "valid".
    EXPECT_FALSE(addr.isValid());
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
    HfLinkAddress parsed = HfLinkAddress::parse(original.toString());
    
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