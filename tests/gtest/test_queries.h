//
// Created by Shinnosuke Kawai on 3/20/26.
//

#pragma once
#include <string_view>

namespace database::testing {

    // $1=col_bool, $2=col_int16, $3=col_int32, $4=col_int64,
    // $5=col_uint16, $6=col_uint32, $7=col_uint64,
    // $8=col_float, $9=col_double, $10=col_text, $11=col_byte, $12=col_ts
    inline constexpr std::string_view insert_query =
        "INSERT INTO test_tables "
        "(col_bool, col_int16, col_int32, col_int64, "
        "col_uint16, col_uint32, col_uint64, "
        "col_float, col_double, col_text, col_byte, col_ts) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)";

    // $1=col_bool, $2=col_int16, $3=col_int32, $4=col_int64,
    // $5=col_uint16, $6=col_uint32, $7=col_uint64,
    // $8=col_float, $9=col_double, $10=col_text, $11=col_byte, $12=col_ts,
    // $13=id (WHERE clause)
    inline constexpr std::string_view update_query =
        "UPDATE test_tables SET "
        "col_bool=$1, col_int16=$2, col_int32=$3, col_int64=$4, "
        "col_uint16=$5, col_uint32=$6, col_uint64=$7, "
        "col_float=$8, col_double=$9, col_text=$10, col_byte=$11, col_ts=$12 "
        "WHERE id=$13";

    // $1=id
    inline constexpr std::string_view select_by_id_query =
        "SELECT id, col_bool, col_int16, col_int32, col_int64, "
        "col_uint16, col_uint32, col_uint64, "
        "col_float, col_double, col_text, col_byte, col_ts "
        "FROM test_tables WHERE id=$1";

    inline constexpr std::string_view select_all_query =
        "SELECT id, col_bool, col_int16, col_int32, col_int64, "
        "col_uint16, col_uint32, col_uint64, "
        "col_float, col_double, col_text, col_byte, col_ts "
        "FROM test_tables";

} // namespace database::testing
