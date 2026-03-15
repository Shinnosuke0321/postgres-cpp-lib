CREATE TABLE IF NOT EXISTS test_tables (
    id SERIAL PRIMARY KEY,
    unnullable_text TEXT NOT NULL,
    nullable_text TEXT,

    unnullable_varchar VARCHAR(255) NOT NULL,
    nullable_varchar VARCHAR(255),

    unnullable_float8 DOUBLE PRECISION NOT NULL,
    nullable_float8 DOUBLE PRECISION,

    unnullable_float4 REAL NOT NULL,
    nullable_float4 REAL,

    unnullable_bool BOOLEAN NOT NULL,
    nullable_bool BOOLEAN,

    unnullable_int16 INT2 NOT NULL,
    nullable_int16 INT2,
    unnullable_int32 INT4 NOT NULL,
    nullable_int32 int4,
    unnullable_int64 INT8 NOT NULL,
    nullable_int64 INT8,

    unnullable_timestamp TIMESTAMP WITH TIME ZONE NOT NULL default now(),
    nullable_timestamp TIMESTAMP WITH TIME ZONE
);