//
// Unit tests for Core::Database::ConnectionError
//
#include <gtest/gtest.h>
#include <database/connection.h>
#include <string>

using namespace Core::Database;

TEST(ConnectionErrorTest, AllSixFactoryMethods_ReturnCorrectCode) {
    ASSERT_EQ(ConnectionError::ConnectionFailed("msg").get_code(),    ConnectionError::Type::ConnectionFailed);
    ASSERT_EQ(ConnectionError::MissingConfig("msg").get_code(),       ConnectionError::Type::MissingConfig);
    ASSERT_EQ(ConnectionError::FactoryNotRegistered("msg").get_code(),ConnectionError::Type::FactoryNotRegistered);
    ASSERT_EQ(ConnectionError::Timeout("msg").get_code(),             ConnectionError::Type::Timeout);
    ASSERT_EQ(ConnectionError::SocketFailed("msg").get_code(),        ConnectionError::Type::SocketFailed);
    ASSERT_EQ(ConnectionError::AuthFailed("msg").get_code(),          ConnectionError::Type::AuthFailed);
}

TEST(ConnectionErrorTest, ToStr_ContainsTypeAndMessage) {
    {
        auto err = ConnectionError::ConnectionFailed("conn fail msg");
        auto s = err.to_str();
        ASSERT_NE(s.find("ConnectionFailed"), std::string::npos);
        ASSERT_NE(s.find("conn fail msg"), std::string::npos);
    }
    {
        auto err = ConnectionError::MissingConfig("cfg msg");
        auto s = err.to_str();
        ASSERT_NE(s.find("MissingConfig"), std::string::npos);
        ASSERT_NE(s.find("cfg msg"), std::string::npos);
    }
    {
        auto err = ConnectionError::FactoryNotRegistered("factory msg");
        auto s = err.to_str();
        ASSERT_NE(s.find("FactoryNotRegistered"), std::string::npos);
        ASSERT_NE(s.find("factory msg"), std::string::npos);
    }
    {
        auto err = ConnectionError::Timeout("timeout msg");
        auto s = err.to_str();
        ASSERT_NE(s.find("Timeout"), std::string::npos);
        ASSERT_NE(s.find("timeout msg"), std::string::npos);
    }
    {
        auto err = ConnectionError::SocketFailed("socket msg");
        auto s = err.to_str();
        ASSERT_NE(s.find("SocketFailed"), std::string::npos);
        ASSERT_NE(s.find("socket msg"), std::string::npos);
    }
    {
        auto err = ConnectionError::AuthFailed("auth msg");
        auto s = err.to_str();
        ASSERT_NE(s.find("AuthFailed"), std::string::npos);
        ASSERT_NE(s.find("auth msg"), std::string::npos);
    }
}

TEST(ConnectionErrorTest, ToStr_EmptyMessage_NoFormatPanic) {
    auto err = ConnectionError::Timeout("");
    const auto s = err.to_str();
    ASSERT_FALSE(s.empty());
    ASSERT_NE(s.find("Timeout"), std::string::npos);
}

TEST(ConnectionErrorTest, GetCode_IsCallableOnConst) {
    const auto err = ConnectionError::AuthFailed("x");
    ASSERT_EQ(err.get_code(), ConnectionError::Type::AuthFailed);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
