#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "MockTransportSdk.h"
#include "PluginHFTransport.h"

using namespace hf_transport;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

// Exposes the protected helpers the production class shares with its subclasses.
class TestableTransport : public PluginHFTransport {
public:
    explicit TestableTransport(ITransportSdk *sdk) : PluginHFTransport(sdk, "default") {}
    using PluginHFTransport::preLinkCreate;
    using PluginHFTransport::generateRepoId;
};

class PluginHFTransportTest : public ::testing::Test {
protected:
    PluginHFTransportTest() {
        channelProps.maxLinks = 10;
        channelProps.currentRole.roleName = "default";
        channelProps.currentRole.linkSide = LS_BOTH;
        ON_CALL(sdk, getActivePersona()).WillByDefault(Return("test-persona"));
        ON_CALL(sdk, getChannelProperties()).WillByDefault(Invoke([this] { return channelProps; }));
        ON_CALL(sdk, requestPluginUserInput("hf_token", _, _))
            .WillByDefault(Return(ChannelResponse{CM_OK, 1}));
        ON_CALL(sdk, requestPluginUserInput("hf_username", _, _))
            .WillByDefault(Return(ChannelResponse{CM_OK, 2}));
    }

    NiceMock<MockTransportSdk> sdk;
    ChannelProperties channelProps;
};

}  // namespace

TEST_F(PluginHFTransportTest, constructor_requests_token_and_username) {
    EXPECT_CALL(sdk, requestPluginUserInput("hf_token", _, true));
    EXPECT_CALL(sdk, requestPluginUserInput("hf_username", _, false));
    TestableTransport transport(&sdk);
}

TEST_F(PluginHFTransportTest, starts_after_both_answers_arrive) {
    TestableTransport transport(&sdk);
    EXPECT_CALL(sdk, updateState(COMPONENT_STATE_STARTED));
    EXPECT_EQ(COMPONENT_OK, transport.onUserInputReceived(1, true, "token"));
    EXPECT_EQ(COMPONENT_OK, transport.onUserInputReceived(2, true, "testuser"));
}

TEST_F(PluginHFTransportTest, fails_without_a_token) {
    unsetenv("HF_TOKEN");
    TestableTransport transport(&sdk);
    EXPECT_CALL(sdk, updateState(COMPONENT_STATE_FAILED));
    transport.onUserInputReceived(1, false, "");
    transport.onUserInputReceived(2, true, "testuser");
}

TEST_F(PluginHFTransportTest, transport_properties_declare_post_and_fetch) {
    TestableTransport transport(&sdk);
    auto props = transport.getTransportProperties();
    ASSERT_EQ(1u, props.supportedActions.count("post"));
    ASSERT_EQ(1u, props.supportedActions.count("fetch"));
    EXPECT_EQ("application/octet-stream", props.supportedActions["post"][0]);
}

TEST_F(PluginHFTransportTest, repo_ids_are_unique_and_prefixed) {
    TestableTransport transport(&sdk);
    std::string first = transport.generateRepoId();
    std::string second = transport.generateRepoId();
    EXPECT_EQ(0u, first.rfind("transport-", 0));
    EXPECT_NE(first, second);
}

TEST_F(PluginHFTransportTest, links_cannot_be_created_before_the_component_is_ready) {
    TestableTransport transport(&sdk);
    EXPECT_CALL(sdk, onLinkStatusChanged(5u, "link-1", LINK_DESTROYED, _));
    EXPECT_FALSE(transport.preLinkCreate("test: ", 5, "link-1", LS_LOADER));
}

TEST_F(PluginHFTransportTest, createLink_before_ready_is_an_error) {
    TestableTransport transport(&sdk);
    EXPECT_EQ(COMPONENT_ERROR, transport.createLink(5, "link-1"));
}

TEST_F(PluginHFTransportTest, loadLinkAddresses_is_unsupported) {
    TestableTransport transport(&sdk);
    EXPECT_EQ(COMPONENT_ERROR, transport.loadLinkAddresses(5, "link-1", {"a"}));
}

TEST_F(PluginHFTransportTest, action_params_match_the_action_type) {
    TestableTransport transport(&sdk);
    auto post = transport.getActionParams(Action{0.0, 1, R"({"linkId":"*","type":"post"})"});
    ASSERT_EQ(1u, post.size());
    EXPECT_TRUE(post[0].encodePackage);
    auto fetch = transport.getActionParams(Action{0.0, 2, R"({"linkId":"*","type":"fetch"})"});
    ASSERT_EQ(1u, fetch.size());
    EXPECT_FALSE(fetch[0].encodePackage);
    EXPECT_TRUE(transport.getActionParams(Action{0.0, 3, R"({"linkId":"*","type":"cleanup"})"}).empty());
}
