//
// Created by Shinnosuke Kawai on 3/24/26.
//

#pragma once
#include <string>
#include <vector>

namespace database::internal {
    // Parses SQL text into individual statements, stripping -- and /* */ comments.
    // Respects single-quoted string literals so comment markers inside strings are ignored.
    // Whitespace normalisation: any run of spaces/tabs/CR/LF is collapsed to a single
    // space; spaces immediately after '(' or before ')' are suppressed.
    inline std::vector<std::string> ParseStatements(const std::string& sql) {
        std::vector<std::string> statements;
        std::string current;
        std::size_t i = 0;
        const std::size_t n = sql.size();

        // Append a space only when it won't be redundant:
        //   - not at the start of a statement
        //   - not after an open paren
        //   - not when a space is already the last character
        auto push_space = [&] {
            if (!current.empty() && current.back() != ' ' && current.back() != '(')
                current += ' ';
        };

        auto flush = [&] {
            const auto start = current.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                const auto end = current.find_last_not_of(" \t\r\n");
                statements.push_back(current.substr(start, end - start + 1));
            }
            current.clear();
        };

        while (i < n) {
            // Single-line comment: skip to end of line
            if (i + 1 < n && sql[i] == '-' && sql[i + 1] == '-') {
                while (i < n && sql[i] != '\n')
                    ++i;
                continue;
            }
            // Block comment: skip to */
            if (i + 1 < n && sql[i] == '/' && sql[i + 1] == '*') {
                i += 2;
                while (i + 1 < n && !(sql[i] == '*' && sql[i + 1] == '/'))
                    ++i;
                if (i + 1 < n)
                    i += 2;
                continue;
            }
            // String literal: pass through verbatim, handling '' escape
            if (sql[i] == '\'') {
                current += sql[i++];
                while (i < n) {
                    if (sql[i] == '\'' && i + 1 < n && sql[i + 1] == '\'') {
                        current += sql[i++];
                        current += sql[i++];
                    } else if (sql[i] == '\'') {
                        current += sql[i++];
                        break;
                    } else {
                        current += sql[i++];
                    }
                }
                continue;
            }
            // Whitespace (space, tab, CR, LF): collapse runs to a single space
            if (sql[i] == ' ' || sql[i] == '\t' || sql[i] == '\r' || sql[i] == '\n') {
                push_space();
                ++i;
                continue;
            }
            // Close paren: strip any preceding space so there is none before ')'
            if (sql[i] == ')') {
                if (!current.empty() && current.back() == ' ')
                    current.pop_back();
                current += sql[i++];
                continue;
            }
            // Statement separator: flush current statement
            if (sql[i] == ';') {
                ++i;
                flush();
                continue;
            }
            current += sql[i++];
        }
        // Handle trailing statement without a semicolon
        flush();
        return statements;
    }
}
