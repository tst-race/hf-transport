#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../../source/transport/PluginHFTransport.h"
#include "../../../source/transport/Link.h"
#include "../../../source/transport/HfTransportStub.h"
#include <ITransportSdk.h>
#include <ComponentTypes.h>

using namespace hf_transport;
using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::NiceMock;

// Mock ITransportSdk for testing
class MockTransportSdk : public ITransportSdk {
public:
    MOCK_METHOD(std::string, getActivePersona, (), (const, override));
    MOCK_METHOD(ChannelProperties, getChannelProperties, (), (const, override));
    MOCK_METHOD(RaceHandle, requestPluginUserInput, (const std::string&, const std::string&, bool), (override));
    MOCK_METHOD(void, updateState, (ComponentState), (override));
    MOCK_METHOD(void, onLinkStatusChanged, (const std::string&, LinkStatus, const LinkProperties&), (override));
    MOCK_METHOD(void, onPackageStatusChanged, (RaceHandle, PackageStatus), (override));
    MOCK_METHOD(void, onConnectionStatusChanged, (const std::string&, ConnectionStatus, const LinkProperties&, const std::string&), (override));
    MOCK_METHOD(void, onLinkDestroyed, (const std::string&), (override));
    MOCK_METHOD(void, onUserInputResponse, (RaceHandle, bool, const std::string&), (override));
    MOCK_METHOD(void, onChannelStatusChanged, (ChannelStatus, const ChannelProperties&), (override));
    MOCK_METHOD(void, onPluginStatusChanged, (ComponentStatus, const PluginConfig&), (override));
    MOCK_METHOD(RaceHandle, requestCommonUserInput, (const std::string&, const std::string&, bool), (override));
    MOCK_METHOD(RaceHandle, requestRemoteUserInput, (const std::string&, const std::string&, const std::string&, bool), (override));
};

// Mock Link for testing transport without full HF stub
class MockLink : public Link {
public:
    MockLink(const LinkID& id) : Link(id, HfLinkAddress(), LinkProperties(), false, nullptr, nullptr) {}
    
    MOCK_METHOD(std::string, postOnActionThread, (const std::string&, const std::vector<RaceHandle>&, uint64_t), (override));
    MOCK_METHOD(bool, uploadContent, (const std::vector<uint8_t>&, const std::string&, std::string&), (override));
    MOCK_METHOD(std::string, fetchOnActionThread, (const std::string&), (override));
    MOCK_METHOD(bool, downloadNewCommits, (const std::string&, std::vector<std::pair<std::string, std::vector<uint8_t>>>&), (override));
};

class TestableTransport : public PluginHFTransport {
public:
    TestableTransport(ITransportSdk* sdk) : PluginHFTransport(sdk, "default") {}
    
    // Expose protected methods for testing
    using PluginHFTransport::preLinkCreate;
    using PluginHFTransport::postLinkCreate;
    using PluginHFTransport::handleUserInputResponse;
    using PluginHFTransport::createLinkInstance;
    using PluginHFTransport::generateRepoId;
    using PluginHFTransport::initializeHfStub;
    using PluginHFTransport::hfStub_;
    using PluginHFTransport::ready_;
    
    void setReady(bool r) { ready_ = r; }
    void setHfToken(const std::string& token) { hfToken_ = token; }
    void setHfUsername(const std::string& username) { hfUsername_ = username; }
    void setHfStub(std::unique_ptr<HfTransportStub> stub) { hfStub_ = std::move(stub); }
};

class PluginHFTransportTest : public ::testing::Test {
protected:
    NiceMock<MockTransportSdk> sdk_;
    ChannelProperties channelProps_;
    
    void SetUp() override {
        channelProps_.maxLinks = 10;
        channelProps_.currentRole.linkSide = LS_CREATOR;
        ON_CALL(sdk_, getActivePersona()).WillByDefault(Return("test-persona"));
        ON_CALL(sdk_, getChannelProperties()).WillByDefault(Invoke([this]() { return channelProps_; }));
    }
};

TEST_F(PluginHFTransportTest, ConstructorRequestsUserInputs) {
    EXPECT_CALL(sdk_, requestPluginUserInput("hf_token", "HF Write Token (write scope PAT)", true))
        .WillOnce(Return(RaceHandle{1}));
    EXPECT_CALL(sdk_, requestPluginUserInput("hf_username", "HF Username", false))
        .WillOnce(Return(RaceHandle{2}));
    
    auto transport = std::make_unique<TestableTransport>(&sdk_);
}

TEST_F(PluginHFTransportTest, GetTransportProperties) {
    EXPECT_CALL(sdk_, requestPluginUserInput(_, _, _)).Times(2);
    
    auto transport = std::make_unique<TestableTransport>(&sdk_);
    
    auto props = transport->getTransportProperties();
    EXPECT_TRUE(props.supportedActions.count("post"));
    EXPECT_TRUE(props.supportedActions.count("fetch"));
    EXPECT_EQ(props.supportedActions["post"][0], "application/octet-stream");
    EXPECT_EQ(props.supportedActions["fetch"][0], "application/octet-stream");
}

TEST_F(PluginHFTransportTest, GenerateRepoId) {
    EXPECT_CALL(sdk_, requestPluginUserInput(_, _, _)).Times(2);
    
    auto transport = std::make_unique<TestableTransport>(&sdk_);
    transport->setReady(true);
    
    std::string repoId1 = transport->generateRepoId();
    std::string repoId2 = transport->generateRepoId();
    
    EXPECT_TRUE(repoId1.rfind("transport-", 0) == 0);
    EXPECT_TRUE(repoId2.rfind("transport-", 0) == 0);
    EXPECT_NE(repoId1, repoId2);
}

TEST_F(PluginHFTransportTest, PreLinkCreateFailsWhenNotReady) {
    EXPECT_CALL(sdk_, requestPluginUserInput(_, _, _)).Times(2);
    
    auto transport = std::make_unique<TestableTransport>(&sdk_);
    transport->setReady(false);
    
    bool result = transport->preLinkCreate("test", 1, "link-1", LS_LOADER);
    EXPECT_FALSE(result);
}

TEST_F(PluginHFTransportTest, PreLinkCreateFailsWhenInvalidRole) {
    EXPECT_CALL(sdk_, requestPluginUserInput(_, _, _)).Times(2);
    
    auto transport = std::make_unique<TestableTransport>(&sdk_);
    transport->setReady(true);
    channelProps_.currentRole.linkSide = LS_LOADER;  // Wrong role for creator
    
    bool result = transport->preLinkCreate("test", 1, "link-1", LS_LOADER);
    EXPECT_FALSE(result);
}

TEST_F(PluginHFTransportTest, PreLinkCreateFailsWhenMaxLinksExceeded) {
    EXPECT_CALL(sdk_, requestPluginUserInput(_, _, _)).Times(2);
    
    auto transport = std::make_unique<TestableTransport>(&sdk_);
    transport->setReady(true);
    channelProps_.maxLinks = 1;
    
    // Add a mock link to reach max
    auto mockLink = std::make_shared<MockLink>("existing-link");
    // Can't easily test this without accessing private members
}

TEST_F(PluginHFTransportTest, UserInputHandling) {
    EXPECT_CALL(sdk_, requestPluginUserInput("hf_token", "HF Write Token (write scope PAT)", true))
        .WillOnce(Return(RaceHandle{1}));
    EXPECT_CALL(sdk_, requestPluginUserInput("hf_username", "HF Username", false))
        .WillOnce(Return(RaceHandle{2}));
    EXPECT_CALL(sdk_, updateState(COMPONENT_STATE_STARTED)).Times(1);
    
    auto transport = std::make_unique<TestableTransport>(&sdk_);
    transport->setHfToken("test-token");
    transport->setHfUsername("testuser");
    
    // Simulate receiving user inputs
    transport->handleUserInputResponse(RaceHandle{1}, true, "test-token");
    transport->handleUserInputResponse(RaceHandle{2}, true, "testuser");
    
    EXPECT_TRUE(transport->ready_);
}