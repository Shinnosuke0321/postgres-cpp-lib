#include <gtest/gtest.h>
#include <postgrescxx/internal/sql_parser.h>

using database::internal::ParseStatements;

class ParserTest : public testing::Test {
protected:
    void SetUp() override {
        table_spaces = "CREATE TABLE weather (\n"
                       "          city VARCHAR(80), \n"
                       "          temp_lo INT, -- low temperature \n"
                       "          temp_hi INT, -- high temperature \n"
                       "          prcp REAL, -- precipitation \n"
                       "          date DATE\n"
                       ");";

        table_single_line = "CREATE TABLE accounts ("
                            "user_id SERIAL PRIMARY KEY,"
                            "username VARCHAR(50) UNIQUE NOT NULL,"
                            "password VARCHAR(50) NOT NULL,"
                            "email VARCHAR(255) UNIQUE NOT NULL,"
                            "created_at TIMESTAMP NOT NULL,"
                            "last_login TIMESTAMP);";
    }
    std::string table_spaces;
    std::string table_single_line;
};

TEST_F(ParserTest, EmptyInput) {
    EXPECT_TRUE(ParseStatements("").empty());
}

TEST_F(ParserTest, WhitespaceOnlyInput) {
    EXPECT_TRUE(ParseStatements("   \n\t  ").empty());
}

TEST_F(ParserTest, CommentOnlyInput) {
    EXPECT_TRUE(ParseStatements("-- just a comment\n").empty());
}

TEST_F(ParserTest, BlockCommentOnlyInput) {
    EXPECT_TRUE(ParseStatements("/* just a comment */").empty());
}

TEST_F(ParserTest, SingleStatement) {
    auto stmts = ParseStatements("SELECT 1;");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT 1");
}

TEST_F(ParserTest, WhitespaceTrimming) {
    auto stmts = ParseStatements("  SELECT 1  ;");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT 1");
}

TEST_F(ParserTest, MultipleStatements) {
    auto stmts = ParseStatements("SELECT 1; SELECT 2;");
    ASSERT_EQ(stmts.size(), 2u);
    EXPECT_EQ(stmts[0], "SELECT 1");
    EXPECT_EQ(stmts[1], "SELECT 2");
}

TEST_F(ParserTest, TrailingStatementNoSemicolon) {
    auto stmts = ParseStatements("SELECT 1; SELECT 2");
    ASSERT_EQ(stmts.size(), 2u);
    EXPECT_EQ(stmts[0], "SELECT 1");
    EXPECT_EQ(stmts[1], "SELECT 2");
}

TEST_F(ParserTest, MultiLineStatement) {
    auto stmts = ParseStatements("SELECT\n1\n;");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT 1");
}

// -- single-line comment: stripped; the '\n' after it is treated as whitespace
// and collapsed with the space already present before the comment.
TEST_F(ParserTest, SingleLineComment) {
    auto stmts = ParseStatements("SELECT -- comment\n1;");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT 1");
}

// /* */ block comment: stripped; surrounding whitespace is collapsed to one space.
TEST_F(ParserTest, BlockComment) {
    auto stmts = ParseStatements("SELECT /* comment */ 1;");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT 1");
}

TEST_F(ParserTest, MixedCommentsAndStatements) {
    auto stmts = ParseStatements("SELECT 1; -- pick one\nSELECT 2;");
    ASSERT_EQ(stmts.size(), 2u);
    EXPECT_EQ(stmts[0], "SELECT 1");
    EXPECT_EQ(stmts[1], "SELECT 2");
}

// '--' inside a string literal must NOT be treated as a comment.
TEST_F(ParserTest, StringLiteralWithDashDash) {
    auto stmts = ParseStatements("SELECT '--not a comment';");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT '--not a comment'");
}

// '/* */' inside a string literal must NOT be treated as a block comment.
TEST_F(ParserTest, StringLiteralWithBlockComment) {
    auto stmts = ParseStatements("SELECT '/* not */ a comment';");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT '/* not */ a comment'");
}

// '' is the SQL escape for a literal single-quote inside a string.
TEST_F(ParserTest, EscapedSingleQuote) {
    auto stmts = ParseStatements("SELECT 'it''s';");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT 'it''s'");
}

// ';' inside a string literal must NOT split the statement.
TEST_F(ParserTest, SemicolonInsideString) {
    auto stmts = ParseStatements("SELECT 'a;b';");
    ASSERT_EQ(stmts.size(), 1u);
    EXPECT_EQ(stmts[0], "SELECT 'a;b'");
}

TEST_F(ParserTest, FullTableCreationQueryWithSpacesAndNewlines) {
    auto stmts = ParseStatements(table_spaces);
    ASSERT_EQ(stmts.size(), 1u);
    ASSERT_EQ(stmts[0], "CREATE TABLE weather (city VARCHAR(80), temp_lo INT, temp_hi INT, prcp REAL, date DATE)");
}

TEST_F(ParserTest, SingleLineTable) {
    const auto stmts = ParseStatements(table_single_line);
    ASSERT_EQ(stmts.size(), 1u);
    ASSERT_EQ(stmts[0], "CREATE TABLE accounts ("
                        "user_id SERIAL PRIMARY KEY,"
                        "username VARCHAR(50) UNIQUE NOT NULL,"
                        "password VARCHAR(50) NOT NULL,"
                        "email VARCHAR(255) UNIQUE NOT NULL,"
                        "created_at TIMESTAMP NOT NULL,"
                        "last_login TIMESTAMP)");
}

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
