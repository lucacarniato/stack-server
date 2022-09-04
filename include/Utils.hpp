#pragma once

namespace stack_server
{
    /// \brief Compute the integer from a byte representation
    /// \param[in] buff The input byte to convert
    static int compute_int_from_byte(const char& buff)
    {
        char mask = 0x01;
        int result = 0;
        for (int i = 0; i < 8; ++i)
        {
            const auto mask_result = buff & mask;
            result = result + mask_result;
            mask = mask << 1;
        }
        return result;
    }
}
