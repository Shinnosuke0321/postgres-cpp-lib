//
// Created by Shinnosuke Kawai on 4/25/26.
//

#pragma once

namespace postgres_cxx {
    class io_tracker {
    public:
        virtual ~io_tracker() = default;
    private:
        virtual void on_write_event() noexcept = 0;
        virtual void on_read_event() noexcept = 0;
        virtual void handle_read() noexcept = 0;
        virtual void handle_write() noexcept = 0;
        virtual void reset_events() noexcept = 0;
    };
}