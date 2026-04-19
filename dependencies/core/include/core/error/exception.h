//
// Created by Shinnosuke Kawai on 4/10/26.
//
#pragma once
#include <string>
#include <format>

namespace core::error {
    class exception {
    public:
        explicit exception(std::string category, std::string code_str, std::string message) noexcept
        : m_code_str(std::move(code_str)),
          m_category(std::move(category)),
          m_message(std::move(message)) {}

        [[nodiscard]]
        std::string to_what() const noexcept {
            return std::format("[{}:{}] {}", m_category, m_code_str, m_message);
        }
    private:
        std::string m_code_str;
        std::string m_category;
        std::string m_message;
    };
}