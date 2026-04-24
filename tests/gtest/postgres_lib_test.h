//
// Created by Shinnosuke Kawai on 3/26/26.
//

#pragma once
#include "test_values.h"
#include <postgrescxx/client.h>
#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
inline int setenv(const char* name, const char* value, int overwrite)
{
    if (!overwrite) {
        size_t envsize = 0;
        errno_t err = getenv_s(&envsize, NULL, 0, name);
        if (err == 0 && envsize != 0) return 0; // Variable already exists, do not overwrite
    }
    std::string env_var = std::string(name) + "=" + std::string(value);
    return _putenv(env_var.c_str());
}
#else
#include <cstdlib> // for setenv on non-Windows systems
#endif

class DbConnectionTest : public testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    std::string valid_url = "postgresql://test_user:test_password@localhost:5432/test_db?sslmode=disable";
    std::string invalid_url_user = "postgresql://fake_user:test_password@localhost:5432/test_db?sslmode=disable";
    std::string invalid_url_password = "postgresql://test_user:fake_password@localhost:5432/test_db?sslmode=disable";
    std::string invalid_url_port = "postgresql://test_user:test_password@localhost:12345/test_db?sslmode=disable";
    std::string invalid_url_db = "postgresql://test_user:test_password@localhost:5432/fake_db?sslmode=disable";
    std::string invalid_url_host = "postgresql://test_user:test_password@fake_host:5432/test_db?sslmode=disable";
};
