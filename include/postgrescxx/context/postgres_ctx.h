//
// Created by Shinnosuke Kawai on 4/25/26.
//
#pragma once
#include <core/error/base_error.h>
#include <core/memory/intrusive_ptr.h>
#include <libpq-fe.h>

  namespace postgres_cxx {
      class postgres_ctx : public core::ref_counted<postgres_ctx> {
      public:
          explicit postgres_ctx(PGconn* conn) noexcept : m_conn(conn) {}
          PGconn* raw_conn() const noexcept { return m_conn; }
          postgres_ctx& operator=(postgres_ctx&& other) noexcept {
              if (this == &other) return *this;
              m_conn = other.m_conn;
              other.m_conn = nullptr;
              return *this;
          }
          postgres_ctx(postgres_ctx&& other) noexcept : m_conn(other.m_conn) {
              other.m_conn = nullptr;
          }
          COPY_SEMANTICS(postgres_ctx, delete);
          ~postgres_ctx() override {
              if (m_conn) PQfinish(m_conn);
          }
      private:
          PGconn* m_conn;
      };
  }