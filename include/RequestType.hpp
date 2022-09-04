#pragma once

namespace stack_server
{
    /// \brief The type of client requests
    enum class RequestType
    {
        Push,
        Pop,
        None
    };

} // namespace stack_server
