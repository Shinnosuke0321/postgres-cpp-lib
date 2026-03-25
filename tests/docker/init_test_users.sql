DROP TABLE IF EXISTS test_tables;

CREATE TABLE test_tables (
    id SERIAL PRIMARY KEY,
    col_bool   BOOLEAN,                    -- bool
    col_int16  SMALLINT,                   -- int16_t
    col_int32  INTEGER,                    -- int32_t
    col_int64  BIGINT,                     -- int64_t
    col_uint16 INTEGER,                    -- uint16_t (no native unsigned; INTEGER holds all values)
    col_uint32 BIGINT,                     -- uint32_t (BIGINT holds all values)
    col_uint64 NUMERIC(20, 0),             -- uint64_t (NUMERIC holds all values)
    col_float  REAL,                       -- float
    col_double DOUBLE PRECISION,           -- double
    col_text   TEXT,                       -- const char*, char*, std::string (all nullable for nullptr_t)
    col_byte   BYTEA,                      -- std::byte
    col_ts     TIMESTAMP WITHOUT TIME ZONE -- database::timestamp (system_clock::time_point)
);