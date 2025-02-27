// Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef ROCPRIM_DEVICE_DEVICE_SEGMENTED_RADIX_SORT_HPP_
#define ROCPRIM_DEVICE_DEVICE_SEGMENTED_RADIX_SORT_HPP_

#include <iostream>
#include <iterator>
#include <type_traits>
#include <utility>

#include "../config.hpp"
#include "../detail/various.hpp"
#include "../detail/radix_sort.hpp"

#include "../intrinsics.hpp"
#include "../functional.hpp"
#include "../types.hpp"

#include "../block/block_load.hpp"
#include "../iterator/counting_iterator.hpp"
#include "device_segmented_radix_sort_config.hpp"
#include "device_partition.hpp"
#include "detail/device_segmented_radix_sort.hpp"

/// \addtogroup devicemodule
/// @{

BEGIN_ROCPRIM_NAMESPACE

namespace detail
{

template<
    class Config,
    bool Descending,
    unsigned int BlockSize,
    class KeysInputIterator,
    class KeysOutputIterator,
    class ValuesInputIterator,
    class ValuesOutputIterator,
    class OffsetIterator
>
ROCPRIM_KERNEL
__launch_bounds__(BlockSize)
void segmented_sort_kernel(KeysInputIterator keys_input,
                           typename std::iterator_traits<KeysInputIterator>::value_type * keys_tmp,
                           KeysOutputIterator keys_output,
                           ValuesInputIterator values_input,
                           typename std::iterator_traits<ValuesInputIterator>::value_type * values_tmp,
                           ValuesOutputIterator values_output,
                           bool to_output,
                           OffsetIterator begin_offsets,
                           OffsetIterator end_offsets,
                           unsigned int long_iterations,
                           unsigned int short_iterations,
                           unsigned int begin_bit,
                           unsigned int end_bit)
{
    segmented_sort<Config, Descending>(
        keys_input, keys_tmp, keys_output, values_input, values_tmp, values_output,
        to_output,
        begin_offsets, end_offsets,
        long_iterations, short_iterations,
        begin_bit, end_bit
    );
}

template<
    class Config,
    bool Descending,
    unsigned int BlockSize,
    class KeysInputIterator,
    class KeysOutputIterator,
    class ValuesInputIterator,
    class ValuesOutputIterator,
    class SegmentIndexIterator,
    class OffsetIterator
>
ROCPRIM_KERNEL
__launch_bounds__(BlockSize)
void segmented_sort_large_kernel(KeysInputIterator keys_input,
                                 typename std::iterator_traits<KeysInputIterator>::value_type * keys_tmp,
                                 KeysOutputIterator keys_output,
                                 ValuesInputIterator values_input,
                                 typename std::iterator_traits<ValuesInputIterator>::value_type * values_tmp,
                                 ValuesOutputIterator values_output,
                                 bool to_output,
                                 SegmentIndexIterator segment_indices,
                                 OffsetIterator begin_offsets,
                                 OffsetIterator end_offsets,
                                 unsigned int long_iterations,
                                 unsigned int short_iterations,
                                 unsigned int begin_bit,
                                 unsigned int end_bit)
{
    segmented_sort_large<Config, Descending>(
        keys_input, keys_tmp, keys_output, values_input, values_tmp, values_output,
        to_output, segment_indices,
        begin_offsets, end_offsets,
        long_iterations, short_iterations,
        begin_bit, end_bit
    );
}

template<
    class Config,
    bool Descending,
    unsigned int BlockSize,
    class KeysInputIterator,
    class KeysOutputIterator,
    class ValuesInputIterator,
    class ValuesOutputIterator,
    class SegmentIndexIterator,
    class OffsetIterator
>
ROCPRIM_KERNEL
__launch_bounds__(BlockSize)
void segmented_sort_small_kernel(KeysInputIterator keys_input,
                                 typename std::iterator_traits<KeysInputIterator>::value_type * keys_tmp,
                                 KeysOutputIterator keys_output,
                                 ValuesInputIterator values_input,
                                 typename std::iterator_traits<ValuesInputIterator>::value_type * values_tmp,
                                 ValuesOutputIterator values_output,
                                 bool to_output,
                                 unsigned int num_segments,
                                 SegmentIndexIterator segment_indices,
                                 OffsetIterator begin_offsets,
                                 OffsetIterator end_offsets,
                                 unsigned int begin_bit,
                                 unsigned int end_bit)
{
    segmented_sort_small<Config, Descending>(
        keys_input, keys_tmp, keys_output, values_input, values_tmp, values_output,
        to_output, num_segments, segment_indices,
        begin_offsets, end_offsets,
        begin_bit, end_bit
    );
}

#define ROCPRIM_DETAIL_HIP_SYNC_AND_RETURN_ON_ERROR(name, size, start) \
    { \
        auto _error = hipGetLastError(); \
        if(_error != hipSuccess) return _error; \
        if(debug_synchronous) \
        { \
            std::cout << name << "(" << size << ")"; \
            auto __error = hipStreamSynchronize(stream); \
            if(__error != hipSuccess) return __error; \
            auto _end = std::chrono::high_resolution_clock::now(); \
            auto _d = std::chrono::duration_cast<std::chrono::duration<double>>(_end - start); \
            std::cout << " " << _d.count() * 1000 << " ms" << '\n'; \
        } \
    }

template<
    class Config,
    bool Descending,
    class KeysInputIterator,
    class KeysOutputIterator,
    class ValuesInputIterator,
    class ValuesOutputIterator,
    class OffsetIterator
>
inline
hipError_t segmented_radix_sort_impl(void * temporary_storage,
                                     size_t& storage_size,
                                     KeysInputIterator keys_input,
                                     typename std::iterator_traits<KeysInputIterator>::value_type * keys_tmp,
                                     KeysOutputIterator keys_output,
                                     ValuesInputIterator values_input,
                                     typename std::iterator_traits<ValuesInputIterator>::value_type * values_tmp,
                                     ValuesOutputIterator values_output,
                                     unsigned int size,
                                     bool& is_result_in_output,
                                     unsigned int segments,
                                     OffsetIterator begin_offsets,
                                     OffsetIterator end_offsets,
                                     unsigned int begin_bit,
                                     unsigned int end_bit,
                                     hipStream_t stream,
                                     bool debug_synchronous)
{
    using key_type = typename std::iterator_traits<KeysInputIterator>::value_type;
    using value_type = typename std::iterator_traits<ValuesInputIterator>::value_type;
    using segment_index_type = unsigned int;
    using segment_index_iterator = counting_iterator<segment_index_type>;

    static_assert(
        std::is_same<key_type, typename std::iterator_traits<KeysOutputIterator>::value_type>::value,
        "KeysInputIterator and KeysOutputIterator must have the same value_type"
    );
    static_assert(
        std::is_same<value_type, typename std::iterator_traits<ValuesOutputIterator>::value_type>::value,
        "ValuesInputIterator and ValuesOutputIterator must have the same value_type"
    );

    using config = default_or_custom_config<
        Config,
        default_segmented_radix_sort_config<ROCPRIM_TARGET_ARCH, key_type, value_type>
    >;

    static constexpr bool with_values = !std::is_same<value_type, ::rocprim::empty_type>::value;
    static constexpr bool partitioning_allowed =
        !std::is_same<typename config::warp_sort_config, DisabledWarpSortConfig>::value;
    static constexpr unsigned int max_small_segment_length =
        config::warp_sort_config::items_per_thread * config::warp_sort_config::logical_warp_size;
    static constexpr unsigned int small_segments_per_block =
        config::warp_sort_config::block_size / config::warp_sort_config::logical_warp_size;
    const auto large_segment_selector = [=](const unsigned int segment_index) mutable -> bool
    {
        const unsigned int segment_length = end_offsets[segment_index] - begin_offsets[segment_index];
        return segment_length > max_small_segment_length;
    };

    const bool with_double_buffer = keys_tmp != nullptr;
    const unsigned int bits = end_bit - begin_bit;
    const unsigned int iterations = ::rocprim::detail::ceiling_div(bits, config::long_radix_bits);
    const bool to_output = with_double_buffer || (iterations - 1) % 2 == 0;
    is_result_in_output = (iterations % 2 == 0) != to_output;
    const unsigned int radix_bits_diff = config::long_radix_bits - config::short_radix_bits;
    const unsigned int short_iterations = radix_bits_diff != 0
        ? ::rocprim::min(iterations, (config::long_radix_bits * iterations - bits) / radix_bits_diff)
        : 0;
    const unsigned int long_iterations = iterations - short_iterations;
    const bool do_partitioning = partitioning_allowed
        && segments >= config::warp_sort_config::partitioning_threshold;

    const size_t keys_bytes = ::rocprim::detail::align_size(size * sizeof(key_type));
    const size_t values_bytes = with_values ? ::rocprim::detail::align_size(size * sizeof(value_type)) : 0;
    const size_t segment_indices_bytes = ::rocprim::detail::align_size(segments * sizeof(segment_index_type));
    const size_t large_segment_count_bytes = ::rocprim::detail::align_size(sizeof(segment_index_type));

    segment_index_type* large_segment_indices_output{};
    segment_index_type* large_segment_count_output{};
    size_t partition_storage_size{};
    if(temporary_storage == nullptr)
    {
        storage_size = with_double_buffer ? 0 : (keys_bytes + values_bytes);
        if(do_partitioning)
        {
            storage_size += segment_indices_bytes + large_segment_count_bytes;
            const auto partition_result = partition(
                nullptr, partition_storage_size, segment_index_iterator{},
                large_segment_indices_output, large_segment_count_output, segments,
                large_segment_selector, stream, debug_synchronous
            );
            if(hipSuccess != partition_result)
            {
                return partition_result;
            }
            storage_size += partition_storage_size;
        }

        // Make sure user won't try to allocate 0 bytes memory, otherwise
        // user may again pass nullptr as temporary_storage
        storage_size = storage_size == 0 ? 4 : storage_size;
        return hipSuccess;
    }
    if(segments == 0u)
    {
        return hipSuccess;
    }
    if(debug_synchronous)
    {
        std::cout << "begin_bit " << begin_bit << '\n';
        std::cout << "end_bit " << end_bit << '\n';
        std::cout << "bits " << bits << '\n';
        std::cout << "segments " << segments << '\n';
        std::cout << "radix_bits_diff " << radix_bits_diff << '\n';
        std::cout << "storage_size " << storage_size << '\n';
        std::cout << "iterations " << iterations << '\n';
        std::cout << "long_iterations " << long_iterations << '\n';
        std::cout << "short_iterations " << short_iterations << '\n';
        std::cout << "do_partitioning " << do_partitioning << '\n';
        std::cout << "config::sort::block_size: " << config::sort::block_size << '\n';
        std::cout << "config::sort::items_per_thread: " << config::sort::items_per_thread << '\n';
        hipError_t error = hipStreamSynchronize(stream);
        if(error != hipSuccess) return error;
    }

    char* ptr = reinterpret_cast<char*>(temporary_storage);
    if(!with_double_buffer)
    {
        keys_tmp = reinterpret_cast<key_type*>(ptr);
        ptr += keys_bytes;
        values_tmp = with_values ? reinterpret_cast<value_type*>(ptr) : nullptr;
        ptr += values_bytes;
    }
    large_segment_indices_output = reinterpret_cast<segment_index_type*>(ptr);
    ptr += segment_indices_bytes;
    large_segment_count_output = reinterpret_cast<segment_index_type*>(ptr);
    ptr += large_segment_count_bytes;
    auto* partition_storage = ptr;

    if(do_partitioning)
    {
        hipError_t result = partition(
            partition_storage, partition_storage_size, segment_index_iterator{},
            large_segment_indices_output, large_segment_count_output, segments,
            large_segment_selector, stream, debug_synchronous
        );
        if(hipSuccess != result)
        {
            return result;
        }
        segment_index_type large_segment_count{};
        result = hipMemcpyAsync(
            &large_segment_count,
            large_segment_count_output,
            sizeof(segment_index_type),
            hipMemcpyDeviceToHost,
            stream
        );
        if(hipSuccess != result)
        {
            return result;
        }
        result = hipStreamSynchronize(stream);
        if(hipSuccess != result)
        {
            return result;
        }
        if(large_segment_count > 0)
        {
            std::chrono::high_resolution_clock::time_point start;
            if(debug_synchronous) start = std::chrono::high_resolution_clock::now();
            hipLaunchKernelGGL(
                HIP_KERNEL_NAME(segmented_sort_large_kernel<config, Descending, config::sort::block_size>),
                dim3(large_segment_count), dim3(config::sort::block_size), 0, stream,
                keys_input, keys_tmp, keys_output, values_input, values_tmp, values_output,
                to_output, large_segment_indices_output,
                begin_offsets, end_offsets,
                long_iterations, short_iterations,
                begin_bit, end_bit
            );
            ROCPRIM_DETAIL_HIP_SYNC_AND_RETURN_ON_ERROR("segmented_sort:large_segments",
                                                        large_segment_count,
                                                        start)
        }
        const auto small_segment_count = segments - large_segment_count;
        if(small_segment_count > 0)
        {
            const auto small_segment_grid_size = ::rocprim::detail::ceiling_div(small_segment_count,
                                                                                small_segments_per_block);
            std::chrono::high_resolution_clock::time_point start;
            if(debug_synchronous) start = std::chrono::high_resolution_clock::now();
            hipLaunchKernelGGL(
                HIP_KERNEL_NAME(
                    segmented_sort_small_kernel<
                        typename config::warp_sort_config,
                        Descending,
                        config::warp_sort_config::block_size
                    >
                ),
                dim3(small_segment_grid_size), dim3(config::warp_sort_config::block_size), 0, stream,
                keys_input, keys_tmp, keys_output, values_input, values_tmp, values_output,
                is_result_in_output,
                small_segment_count, large_segment_indices_output + large_segment_count,
                begin_offsets, end_offsets,
                begin_bit, end_bit
            );
            ROCPRIM_DETAIL_HIP_SYNC_AND_RETURN_ON_ERROR("segmented_sort:small_segments",
                                                        small_segment_count,
                                                        start)
        }
    }
    else
    {
        std::chrono::high_resolution_clock::time_point start;
        if(debug_synchronous) start = std::chrono::high_resolution_clock::now();
        hipLaunchKernelGGL(
            HIP_KERNEL_NAME(segmented_sort_kernel<config, Descending, config::sort::block_size>),
            dim3(segments), dim3(config::sort::block_size), 0, stream,
            keys_input, keys_tmp, keys_output, values_input, values_tmp, values_output,
            to_output,
            begin_offsets, end_offsets,
            long_iterations, short_iterations,
            begin_bit, end_bit
        );
        ROCPRIM_DETAIL_HIP_SYNC_AND_RETURN_ON_ERROR("segmented_sort", segments, start)
    }
    return hipSuccess;
}

#undef ROCPRIM_DETAIL_HIP_SYNC_AND_RETURN_ON_ERROR

} // end namespace detail

/// \brief Parallel ascending radix sort primitive for device level.
///
/// \p segmented_radix_sort_keys function performs a device-wide radix sort across multiple,
/// non-overlapping sequences of keys. Function sorts input keys in ascending order.
///
/// \par Overview
/// * The contents of the inputs are not altered by the sorting function.
/// * Returns the required size of \p temporary_storage in \p storage_size
/// if \p temporary_storage in a null pointer.
/// * \p Key type (a \p value_type of \p KeysInputIterator and \p KeysOutputIterator) must be
/// an arithmetic type (that is, an integral type or a floating-point type).
/// * Ranges specified by \p keys_input and \p keys_output must have at least \p size elements.
/// * Ranges specified by \p begin_offsets and \p end_offsets must have
/// at least \p segments elements. They may use the same sequence <tt>offsets</tt> of at least
/// <tt>segments + 1</tt> elements: <tt>offsets</tt> for \p begin_offsets and
/// <tt>offsets + 1</tt> for \p end_offsets.
/// * If \p Key is an integer type and the range of keys is known in advance, the performance
/// can be improved by setting \p begin_bit and \p end_bit, for example if all keys are in range
/// [100, 10000], <tt>begin_bit = 0</tt> and <tt>end_bit = 14</tt> will cover the whole range.
///
/// \tparam Config - [optional] configuration of the primitive. It can be
/// \p segmented_radix_sort_config or a custom class with the same members.
/// \tparam KeysInputIterator - random-access iterator type of the input range. Must meet the
/// requirements of a C++ InputIterator concept. It can be a simple pointer type.
/// \tparam KeysOutputIterator - random-access iterator type of the output range. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
/// \tparam OffsetIterator - random-access iterator type of segment offsets. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
///
/// \param [in] temporary_storage - pointer to a device-accessible temporary storage. When
/// a null pointer is passed, the required allocation size (in bytes) is written to
/// \p storage_size and function returns without performing the sort operation.
/// \param [in,out] storage_size - reference to a size (in bytes) of \p temporary_storage.
/// \param [in] keys_input - pointer to the first element in the range to sort.
/// \param [out] keys_output - pointer to the first element in the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] segments - number of segments in the input range.
/// \param [in] begin_offsets - iterator to the first element in the range of beginning offsets.
/// \param [in] end_offsets - iterator to the first element in the range of ending offsets.
/// \param [in] begin_bit - [optional] index of the first (least significant) bit used in
/// key comparison. Must be in range <tt>[0; 8 * sizeof(Key))</tt>. Default value: \p 0.
/// Non-default value not supported for floating-point key-types.
/// \param [in] end_bit - [optional] past-the-end index (most significant) bit used in
/// key comparison. Must be in range <tt>(begin_bit; 8 * sizeof(Key)]</tt>. Default
/// value: \p <tt>8 * sizeof(Key)</tt>. Non-default value not supported for floating-point key-types.
/// \param [in] stream - [optional] HIP stream object. Default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. Default value is \p false.
///
/// \returns \p hipSuccess (\p 0) after successful sort; otherwise a HIP runtime error of
/// type \p hipError_t.
///
/// \par Example
/// \parblock
/// In this example a device-level ascending radix sort is performed on an array of
/// \p float values.
///
/// \code{.cpp}
/// #include <rocprim/rocprim.hpp>
///
/// // Prepare input and output (declare pointers, allocate device memory etc.)
/// size_t input_size;      // e.g., 8
/// float * input;          // e.g., [0.6, 0.3, 0.65, 0.4, 0.2, 0.08, 1, 0.7]
/// float * output;         // empty array of 8 elements
/// unsigned int segments;  // e.g., 3
/// int * offsets;          // e.g. [0, 2, 3, 8]
///
/// size_t temporary_storage_size_bytes;
/// void * temporary_storage_ptr = nullptr;
/// // Get required size of the temporary storage
/// rocprim::segmented_radix_sort_keys(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     input, output, input_size,
///     segments, offsets, offsets + 1
/// );
///
/// // allocate temporary storage
/// hipMalloc(&temporary_storage_ptr, temporary_storage_size_bytes);
///
/// // perform sort
/// rocprim::segmented_radix_sort_keys(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     input, output, input_size,
///     segments, offsets, offsets + 1
/// );
/// // keys_output: [0.3, 0.6, 0.65, 0.08, 0.2, 0.4, 0.7, 1]
/// \endcode
/// \endparblock
template<
    class Config = default_config,
    class KeysInputIterator,
    class KeysOutputIterator,
    class OffsetIterator,
    class Key = typename std::iterator_traits<KeysInputIterator>::value_type
>
inline
hipError_t segmented_radix_sort_keys(void * temporary_storage,
                                     size_t& storage_size,
                                     KeysInputIterator keys_input,
                                     KeysOutputIterator keys_output,
                                     unsigned int size,
                                     unsigned int segments,
                                     OffsetIterator begin_offsets,
                                     OffsetIterator end_offsets,
                                     unsigned int begin_bit = 0,
                                     unsigned int end_bit = 8 * sizeof(Key),
                                     hipStream_t stream = 0,
                                     bool debug_synchronous = false)
{
    empty_type * values = nullptr;
    bool ignored;
    return detail::segmented_radix_sort_impl<Config, false>(
        temporary_storage, storage_size,
        keys_input, nullptr, keys_output,
        values, nullptr, values,
        size, ignored,
        segments, begin_offsets, end_offsets,
        begin_bit, end_bit,
        stream, debug_synchronous
    );
}

/// \brief Parallel descending radix sort primitive for device level.
///
/// \p segmented_radix_sort_keys_desc function performs a device-wide radix sort across multiple,
/// non-overlapping sequences of keys. Function sorts input keys in descending order.
///
/// \par Overview
/// * The contents of the inputs are not altered by the sorting function.
/// * Returns the required size of \p temporary_storage in \p storage_size
/// if \p temporary_storage in a null pointer.
/// * \p Key type (a \p value_type of \p KeysInputIterator and \p KeysOutputIterator) must be
/// an arithmetic type (that is, an integral type or a floating-point type).
/// * Ranges specified by \p keys_input and \p keys_output must have at least \p size elements.
/// * Ranges specified by \p begin_offsets and \p end_offsets must have
/// at least \p segments elements. They may use the same sequence <tt>offsets</tt> of at least
/// <tt>segments + 1</tt> elements: <tt>offsets</tt> for \p begin_offsets and
/// <tt>offsets + 1</tt> for \p end_offsets.
/// * If \p Key is an integer type and the range of keys is known in advance, the performance
/// can be improved by setting \p begin_bit and \p end_bit, for example if all keys are in range
/// [100, 10000], <tt>begin_bit = 0</tt> and <tt>end_bit = 14</tt> will cover the whole range.
///
/// \tparam Config - [optional] configuration of the primitive. It can be
/// \p segmented_radix_sort_config or a custom class with the same members.
/// \tparam KeysInputIterator - random-access iterator type of the input range. Must meet the
/// requirements of a C++ InputIterator concept. It can be a simple pointer type.
/// \tparam KeysOutputIterator - random-access iterator type of the output range. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
/// \tparam OffsetIterator - random-access iterator type of segment offsets. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
///
/// \param [in] temporary_storage - pointer to a device-accessible temporary storage. When
/// a null pointer is passed, the required allocation size (in bytes) is written to
/// \p storage_size and function returns without performing the sort operation.
/// \param [in,out] storage_size - reference to a size (in bytes) of \p temporary_storage.
/// \param [in] keys_input - pointer to the first element in the range to sort.
/// \param [out] keys_output - pointer to the first element in the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] segments - number of segments in the input range.
/// \param [in] begin_offsets - iterator to the first element in the range of beginning offsets.
/// \param [in] end_offsets - iterator to the first element in the range of ending offsets.
/// \param [in] begin_bit - [optional] index of the first (least significant) bit used in
/// key comparison. Must be in range <tt>[0; 8 * sizeof(Key))</tt>. Default value: \p 0.
/// Non-default value not supported for floating-point key-types.
/// \param [in] end_bit - [optional] past-the-end index (most significant) bit used in
/// key comparison. Must be in range <tt>(begin_bit; 8 * sizeof(Key)]</tt>. Default
/// value: \p <tt>8 * sizeof(Key)</tt>. Non-default value not supported for floating-point key-types.
/// \param [in] stream - [optional] HIP stream object. Default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. Default value is \p false.
///
/// \returns \p hipSuccess (\p 0) after successful sort; otherwise a HIP runtime error of
/// type \p hipError_t.
///
/// \par Example
/// \parblock
/// In this example a device-level descending radix sort is performed on an array of
/// integer values.
///
/// \code{.cpp}
/// #include <rocprim/rocprim.hpp>
///
/// // Prepare input and output (declare pointers, allocate device memory etc.)
/// size_t input_size;      // e.g., 8
/// int * input;            // e.g., [6, 3, 5, 4, 2, 8, 1, 7]
/// int * output;           // empty array of 8 elements
/// unsigned int segments;  // e.g., 3
/// int * offsets;          // e.g. [0, 2, 3, 8]
///
/// size_t temporary_storage_size_bytes;
/// void * temporary_storage_ptr = nullptr;
/// // Get required size of the temporary storage
/// rocprim::segmented_radix_sort_keys_desc(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     input, output, input_size,
///     segments, offsets, offsets + 1
/// );
///
/// // allocate temporary storage
/// hipMalloc(&temporary_storage_ptr, temporary_storage_size_bytes);
///
/// // perform sort
/// rocprim::segmented_radix_sort_keys_desc(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     input, output, input_size,
///     segments, offsets, offsets + 1
/// );
/// // keys_output: [6, 3, 5, 8, 7, 4, 2, 1]
/// \endcode
/// \endparblock
template<
    class Config = default_config,
    class KeysInputIterator,
    class KeysOutputIterator,
    class OffsetIterator,
    class Key = typename std::iterator_traits<KeysInputIterator>::value_type
>
inline
hipError_t segmented_radix_sort_keys_desc(void * temporary_storage,
                                          size_t& storage_size,
                                          KeysInputIterator keys_input,
                                          KeysOutputIterator keys_output,
                                          unsigned int size,
                                          unsigned int segments,
                                          OffsetIterator begin_offsets,
                                          OffsetIterator end_offsets,
                                          unsigned int begin_bit = 0,
                                          unsigned int end_bit = 8 * sizeof(Key),
                                          hipStream_t stream = 0,
                                          bool debug_synchronous = false)
{
    empty_type * values = nullptr;
    bool ignored;
    return detail::segmented_radix_sort_impl<Config, true>(
        temporary_storage, storage_size,
        keys_input, nullptr, keys_output,
        values, nullptr, values,
        size, ignored,
        segments, begin_offsets, end_offsets,
        begin_bit, end_bit,
        stream, debug_synchronous
    );
}

/// \brief Parallel ascending radix sort-by-key primitive for device level.
///
/// \p segmented_radix_sort_pairs_desc function performs a device-wide radix sort across multiple,
/// non-overlapping sequences of (key, value) pairs. Function sorts input pairs in ascending order of keys.
///
/// \par Overview
/// * The contents of the inputs are not altered by the sorting function.
/// * Returns the required size of \p temporary_storage in \p storage_size
/// if \p temporary_storage in a null pointer.
/// * \p Key type (a \p value_type of \p KeysInputIterator and \p KeysOutputIterator) must be
/// an arithmetic type (that is, an integral type or a floating-point type).
/// * Ranges specified by \p keys_input, \p keys_output, \p values_input and \p values_output must
/// have at least \p size elements.
/// * Ranges specified by \p begin_offsets and \p end_offsets must have
/// at least \p segments elements. They may use the same sequence <tt>offsets</tt> of at least
/// <tt>segments + 1</tt> elements: <tt>offsets</tt> for \p begin_offsets and
/// <tt>offsets + 1</tt> for \p end_offsets.
/// * If \p Key is an integer type and the range of keys is known in advance, the performance
/// can be improved by setting \p begin_bit and \p end_bit, for example if all keys are in range
/// [100, 10000], <tt>begin_bit = 0</tt> and <tt>end_bit = 14</tt> will cover the whole range.
///
/// \tparam Config - [optional] configuration of the primitive. It can be
/// \p segmented_radix_sort_config or a custom class with the same members.
/// \tparam KeysInputIterator - random-access iterator type of the input range. Must meet the
/// requirements of a C++ InputIterator concept. It can be a simple pointer type.
/// \tparam KeysOutputIterator - random-access iterator type of the output range. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
/// \tparam ValuesInputIterator - random-access iterator type of the input range. Must meet the
/// requirements of a C++ InputIterator concept. It can be a simple pointer type.
/// \tparam ValuesOutputIterator - random-access iterator type of the output range. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
/// \tparam OffsetIterator - random-access iterator type of segment offsets. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
///
/// \param [in] temporary_storage - pointer to a device-accessible temporary storage. When
/// a null pointer is passed, the required allocation size (in bytes) is written to
/// \p storage_size and function returns without performing the sort operation.
/// \param [in,out] storage_size - reference to a size (in bytes) of \p temporary_storage.
/// \param [in] keys_input - pointer to the first element in the range to sort.
/// \param [out] keys_output - pointer to the first element in the output range.
/// \param [in] values_input - pointer to the first element in the range to sort.
/// \param [out] values_output - pointer to the first element in the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] segments - number of segments in the input range.
/// \param [in] begin_offsets - iterator to the first element in the range of beginning offsets.
/// \param [in] end_offsets - iterator to the first element in the range of ending offsets.
/// \param [in] begin_bit - [optional] index of the first (least significant) bit used in
/// key comparison. Must be in range <tt>[0; 8 * sizeof(Key))</tt>. Default value: \p 0.
/// Non-default value not supported for floating-point key-types.
/// \param [in] end_bit - [optional] past-the-end index (most significant) bit used in
/// key comparison. Must be in range <tt>(begin_bit; 8 * sizeof(Key)]</tt>. Default
/// value: \p <tt>8 * sizeof(Key)</tt>. Non-default value not supported for floating-point key-types.
/// \param [in] stream - [optional] HIP stream object. Default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. Default value is \p false.
///
/// \returns \p hipSuccess (\p 0) after successful sort; otherwise a HIP runtime error of
/// type \p hipError_t.
///
/// \par Example
/// \parblock
/// In this example a device-level ascending radix sort is performed where input keys are
/// represented by an array of unsigned integers and input values by an array of <tt>double</tt>s.
///
/// \code{.cpp}
/// #include <rocprim/rocprim.hpp>
///
/// // Prepare input and output (declare pointers, allocate device memory etc.)
/// size_t input_size;          // e.g., 8
/// unsigned int * keys_input;  // e.g., [ 6, 3,  5, 4,  1,  8,  1, 7]
/// double * values_input;      // e.g., [-5, 2, -4, 3, -1, -8, -2, 7]
/// unsigned int * keys_output; // empty array of 8 elements
/// double * values_output;     // empty array of 8 elements
/// unsigned int segments;      // e.g., 3
/// int * offsets;              // e.g. [0, 2, 3, 8]
///
/// // Keys are in range [0; 8], so we can limit compared bit to bits on indexes
/// // 0, 1, 2, 3, and 4. In order to do this begin_bit is set to 0 and end_bit
/// // is set to 5.
///
/// size_t temporary_storage_size_bytes;
/// void * temporary_storage_ptr = nullptr;
/// // Get required size of the temporary storage
/// rocprim::segmented_radix_sort_pairs(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys_input, keys_output, values_input, values_output, input_size,
///     segments, offsets, offsets + 1,
///     0, 5
/// );
///
/// // allocate temporary storage
/// hipMalloc(&temporary_storage_ptr, temporary_storage_size_bytes);
///
/// // perform sort
/// rocprim::segmented_radix_sort_pairs(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys_input, keys_output, values_input, values_output, input_size,
///     segments, offsets, offsets + 1,
///     0, 5
/// );
/// // keys_output:   [3,  6,  5,  1,  1, 4, 7,  8]
/// // values_output: [2, -5, -4, -1, -2, 3, 7, -8]
/// \endcode
/// \endparblock
template<
    class Config = default_config,
    class KeysInputIterator,
    class KeysOutputIterator,
    class ValuesInputIterator,
    class ValuesOutputIterator,
    class OffsetIterator,
    class Key = typename std::iterator_traits<KeysInputIterator>::value_type
>
inline
hipError_t segmented_radix_sort_pairs(void * temporary_storage,
                                      size_t& storage_size,
                                      KeysInputIterator keys_input,
                                      KeysOutputIterator keys_output,
                                      ValuesInputIterator values_input,
                                      ValuesOutputIterator values_output,
                                      unsigned int size,
                                      unsigned int segments,
                                      OffsetIterator begin_offsets,
                                      OffsetIterator end_offsets,
                                      unsigned int begin_bit = 0,
                                      unsigned int end_bit = 8 * sizeof(Key),
                                      hipStream_t stream = 0,
                                      bool debug_synchronous = false)
{
    bool ignored;
    return detail::segmented_radix_sort_impl<Config, false>(
        temporary_storage, storage_size,
        keys_input, nullptr, keys_output,
        values_input, nullptr, values_output,
        size, ignored,
        segments, begin_offsets, end_offsets,
        begin_bit, end_bit,
        stream, debug_synchronous
    );
}

/// \brief Parallel descending radix sort-by-key primitive for device level.
///
/// \p segmented_radix_sort_pairs_desc function performs a device-wide radix sort across multiple,
/// non-overlapping sequences of (key, value) pairs. Function sorts input pairs in descending order of keys.
///
/// \par Overview
/// * The contents of the inputs are not altered by the sorting function.
/// * Returns the required size of \p temporary_storage in \p storage_size
/// if \p temporary_storage in a null pointer.
/// * \p Key type (a \p value_type of \p KeysInputIterator and \p KeysOutputIterator) must be
/// an arithmetic type (that is, an integral type or a floating-point type).
/// * Ranges specified by \p keys_input, \p keys_output, \p values_input and \p values_output must
/// have at least \p size elements.
/// * Ranges specified by \p begin_offsets and \p end_offsets must have
/// at least \p segments elements. They may use the same sequence <tt>offsets</tt> of at least
/// <tt>segments + 1</tt> elements: <tt>offsets</tt> for \p begin_offsets and
/// <tt>offsets + 1</tt> for \p end_offsets.
/// * If \p Key is an integer type and the range of keys is known in advance, the performance
/// can be improved by setting \p begin_bit and \p end_bit, for example if all keys are in range
/// [100, 10000], <tt>begin_bit = 0</tt> and <tt>end_bit = 14</tt> will cover the whole range.
///
/// \tparam Config - [optional] configuration of the primitive. It can be
/// \p segmented_radix_sort_config or a custom class with the same members.
/// \tparam KeysInputIterator - random-access iterator type of the input range. Must meet the
/// requirements of a C++ InputIterator concept. It can be a simple pointer type.
/// \tparam KeysOutputIterator - random-access iterator type of the output range. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
/// \tparam ValuesInputIterator - random-access iterator type of the input range. Must meet the
/// requirements of a C++ InputIterator concept. It can be a simple pointer type.
/// \tparam ValuesOutputIterator - random-access iterator type of the output range. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
/// \tparam OffsetIterator - random-access iterator type of segment offsets. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
///
/// \param [in] temporary_storage - pointer to a device-accessible temporary storage. When
/// a null pointer is passed, the required allocation size (in bytes) is written to
/// \p storage_size and function returns without performing the sort operation.
/// \param [in,out] storage_size - reference to a size (in bytes) of \p temporary_storage.
/// \param [in] keys_input - pointer to the first element in the range to sort.
/// \param [out] keys_output - pointer to the first element in the output range.
/// \param [in] values_input - pointer to the first element in the range to sort.
/// \param [out] values_output - pointer to the first element in the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] segments - number of segments in the input range.
/// \param [in] begin_offsets - iterator to the first element in the range of beginning offsets.
/// \param [in] end_offsets - iterator to the first element in the range of ending offsets.
/// \param [in] begin_bit - [optional] index of the first (least significant) bit used in
/// key comparison. Must be in range <tt>[0; 8 * sizeof(Key))</tt>. Default value: \p 0.
/// Non-default value not supported for floating-point key-types.
/// \param [in] end_bit - [optional] past-the-end index (most significant) bit used in
/// key comparison. Must be in range <tt>(begin_bit; 8 * sizeof(Key)]</tt>. Default
/// value: \p <tt>8 * sizeof(Key)</tt>. Non-default value not supported for floating-point key-types.
/// \param [in] stream - [optional] HIP stream object. Default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. Default value is \p false.
///
/// \returns \p hipSuccess (\p 0) after successful sort; otherwise a HIP runtime error of
/// type \p hipError_t.
///
/// \par Example
/// \parblock
/// In this example a device-level descending radix sort is performed where input keys are
/// represented by an array of integers and input values by an array of <tt>double</tt>s.
///
/// \code{.cpp}
/// #include <rocprim/rocprim.hpp>
///
/// // Prepare input and output (declare pointers, allocate device memory etc.)
/// size_t input_size;       // e.g., 8
/// int * keys_input;        // e.g., [ 6, 3,  5, 4,  1,  8,  1, 7]
/// double * values_input;   // e.g., [-5, 2, -4, 3, -1, -8, -2, 7]
/// int * keys_output;       // empty array of 8 elements
/// double * values_output;  // empty array of 8 elements
/// unsigned int segments;   // e.g., 3
/// int * offsets;           // e.g. [0, 2, 3, 8]
///
/// size_t temporary_storage_size_bytes;
/// void * temporary_storage_ptr = nullptr;
/// // Get required size of the temporary storage
/// rocprim::segmented_radix_sort_pairs_desc(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys_input, keys_output, values_input, values_output,
///     input_size,
///     segments, offsets, offsets + 1
/// );
///
/// // allocate temporary storage
/// hipMalloc(&temporary_storage_ptr, temporary_storage_size_bytes);
///
/// // perform sort
/// rocprim::segmented_radix_sort_pairs_desc(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys_input, keys_output, values_input, values_output,
///     input_size,
///     segments, offsets, offsets + 1
/// );
/// // keys_output:   [ 6, 3,  5,  8, 7, 4,  1,  1]
/// // values_output: [-5, 2, -4, -8, 7, 3, -1, -2]
/// \endcode
/// \endparblock
template<
    class Config = default_config,
    class KeysInputIterator,
    class KeysOutputIterator,
    class ValuesInputIterator,
    class ValuesOutputIterator,
    class OffsetIterator,
    class Key = typename std::iterator_traits<KeysInputIterator>::value_type
>
inline
hipError_t segmented_radix_sort_pairs_desc(void * temporary_storage,
                                           size_t& storage_size,
                                           KeysInputIterator keys_input,
                                           KeysOutputIterator keys_output,
                                           ValuesInputIterator values_input,
                                           ValuesOutputIterator values_output,
                                           unsigned int size,
                                           unsigned int segments,
                                           OffsetIterator begin_offsets,
                                           OffsetIterator end_offsets,
                                           unsigned int begin_bit = 0,
                                           unsigned int end_bit = 8 * sizeof(Key),
                                           hipStream_t stream = 0,
                                           bool debug_synchronous = false)
{
    bool ignored;
    return detail::segmented_radix_sort_impl<Config, true>(
        temporary_storage, storage_size,
        keys_input, nullptr, keys_output,
        values_input, nullptr, values_output,
        size, ignored,
        segments, begin_offsets, end_offsets,
        begin_bit, end_bit,
        stream, debug_synchronous
    );
}

/// \brief Parallel ascending radix sort primitive for device level.
///
/// \p segmented_radix_sort_keys function performs a device-wide radix sort across multiple,
/// non-overlapping sequences of keys. Function sorts input keys in ascending order.
///
/// \par Overview
/// * The contents of both buffers of \p keys may be altered by the sorting function.
/// * \p current() of \p keys is used as the input.
/// * The function will update \p current() of \p keys to point to the buffer
/// that contains the output range.
/// * Returns the required size of \p temporary_storage in \p storage_size
/// if \p temporary_storage in a null pointer.
/// * The function requires small \p temporary_storage as it does not need
/// a temporary buffer of \p size elements.
/// * \p Key type must be an arithmetic type (that is, an integral type or a floating-point
/// type).
/// * Buffers of \p keys must have at least \p size elements.
/// * Ranges specified by \p begin_offsets and \p end_offsets must have
/// at least \p segments elements. They may use the same sequence <tt>offsets</tt> of at least
/// <tt>segments + 1</tt> elements: <tt>offsets</tt> for \p begin_offsets and
/// <tt>offsets + 1</tt> for \p end_offsets.
/// * If \p Key is an integer type and the range of keys is known in advance, the performance
/// can be improved by setting \p begin_bit and \p end_bit, for example if all keys are in range
/// [100, 10000], <tt>begin_bit = 0</tt> and <tt>end_bit = 14</tt> will cover the whole range.
///
/// \tparam Config - [optional] configuration of the primitive. It can be
/// \p segmented_radix_sort_config or a custom class with the same members.
/// \tparam Key - key type. Must be an integral type or a floating-point type.
/// \tparam OffsetIterator - random-access iterator type of segment offsets. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
///
/// \param [in] temporary_storage - pointer to a device-accessible temporary storage. When
/// a null pointer is passed, the required allocation size (in bytes) is written to
/// \p storage_size and function returns without performing the sort operation.
/// \param [in,out] storage_size - reference to a size (in bytes) of \p temporary_storage.
/// \param [in,out] keys - reference to the double-buffer of keys, its \p current()
/// contains the input range and will be updated to point to the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] segments - number of segments in the input range.
/// \param [in] begin_offsets - iterator to the first element in the range of beginning offsets.
/// \param [in] end_offsets - iterator to the first element in the range of ending offsets.
/// \param [in] begin_bit - [optional] index of the first (least significant) bit used in
/// key comparison. Must be in range <tt>[0; 8 * sizeof(Key))</tt>. Default value: \p 0.
/// Non-default value not supported for floating-point key-types.
/// \param [in] end_bit - [optional] past-the-end index (most significant) bit used in
/// key comparison. Must be in range <tt>(begin_bit; 8 * sizeof(Key)]</tt>. Default
/// value: \p <tt>8 * sizeof(Key)</tt>. Non-default value not supported for floating-point key-types.
/// \param [in] stream - [optional] HIP stream object. Default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. Default value is \p false.
///
/// \returns \p hipSuccess (\p 0) after successful sort; otherwise a HIP runtime error of
/// type \p hipError_t.
///
/// \par Example
/// \parblock
/// In this example a device-level ascending radix sort is performed on an array of
/// \p float values.
///
/// \code{.cpp}
/// #include <rocprim/rocprim.hpp>
///
/// // Prepare input and tmp (declare pointers, allocate device memory etc.)
/// size_t input_size;       // e.g., 8
/// float * input;           // e.g., [0.6, 0.3, 0.65, 0.4, 0.2, 0.08, 1, 0.7]
/// float * tmp;             // empty array of 8 elements
/// unsigned int segments;   // e.g., 3
/// int * offsets;           // e.g. [0, 2, 3, 8]
/// // Create double-buffer
/// rocprim::double_buffer<float> keys(input, tmp);
///
/// size_t temporary_storage_size_bytes;
/// void * temporary_storage_ptr = nullptr;
/// // Get required size of the temporary storage
/// rocprim::segmented_radix_sort_keys(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys, input_size,
///     segments, offsets, offsets + 1
/// );
///
/// // allocate temporary storage
/// hipMalloc(&temporary_storage_ptr, temporary_storage_size_bytes);
///
/// // perform sort
/// rocprim::segmented_radix_sort_keys(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys, input_size,
///     segments, offsets, offsets + 1
/// );
/// // keys.current(): [0.3, 0.6, 0.65, 0.08, 0.2, 0.4, 0.7, 1]
/// \endcode
/// \endparblock
template<
    class Config = default_config,
    class Key,
    class OffsetIterator
>
inline
hipError_t segmented_radix_sort_keys(void * temporary_storage,
                                     size_t& storage_size,
                                     double_buffer<Key>& keys,
                                     unsigned int size,
                                     unsigned int segments,
                                     OffsetIterator begin_offsets,
                                     OffsetIterator end_offsets,
                                     unsigned int begin_bit = 0,
                                     unsigned int end_bit = 8 * sizeof(Key),
                                     hipStream_t stream = 0,
                                     bool debug_synchronous = false)
{
    empty_type * values = nullptr;
    bool is_result_in_output;
    hipError_t error = detail::segmented_radix_sort_impl<Config, false>(
        temporary_storage, storage_size,
        keys.current(), keys.current(), keys.alternate(),
        values, values, values,
        size, is_result_in_output,
        segments, begin_offsets, end_offsets,
        begin_bit, end_bit,
        stream, debug_synchronous
    );
    if(temporary_storage != nullptr && is_result_in_output)
    {
        keys.swap();
    }
    return error;
}

/// \brief Parallel descending radix sort primitive for device level.
///
/// \p segmented_radix_sort_keys_desc function performs a device-wide radix sort across multiple,
/// non-overlapping sequences of keys. Function sorts input keys in descending order.
///
/// \par Overview
/// * The contents of both buffers of \p keys may be altered by the sorting function.
/// * \p current() of \p keys is used as the input.
/// * The function will update \p current() of \p keys to point to the buffer
/// that contains the output range.
/// * Returns the required size of \p temporary_storage in \p storage_size
/// if \p temporary_storage in a null pointer.
/// * The function requires small \p temporary_storage as it does not need
/// a temporary buffer of \p size elements.
/// * \p Key type must be an arithmetic type (that is, an integral type or a floating-point
/// type).
/// * Buffers of \p keys must have at least \p size elements.
/// * Ranges specified by \p begin_offsets and \p end_offsets must have
/// at least \p segments elements. They may use the same sequence <tt>offsets</tt> of at least
/// <tt>segments + 1</tt> elements: <tt>offsets</tt> for \p begin_offsets and
/// <tt>offsets + 1</tt> for \p end_offsets.
/// * If \p Key is an integer type and the range of keys is known in advance, the performance
/// can be improved by setting \p begin_bit and \p end_bit, for example if all keys are in range
/// [100, 10000], <tt>begin_bit = 0</tt> and <tt>end_bit = 14</tt> will cover the whole range.
///
/// \tparam Config - [optional] configuration of the primitive. It can be
/// \p segmented_radix_sort_config or a custom class with the same members.
/// \tparam Key - key type. Must be an integral type or a floating-point type.
/// \tparam OffsetIterator - random-access iterator type of segment offsets. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
///
/// \param [in] temporary_storage - pointer to a device-accessible temporary storage. When
/// a null pointer is passed, the required allocation size (in bytes) is written to
/// \p storage_size and function returns without performing the sort operation.
/// \param [in,out] storage_size - reference to a size (in bytes) of \p temporary_storage.
/// \param [in,out] keys - reference to the double-buffer of keys, its \p current()
/// contains the input range and will be updated to point to the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] segments - number of segments in the input range.
/// \param [in] begin_offsets - iterator to the first element in the range of beginning offsets.
/// \param [in] end_offsets - iterator to the first element in the range of ending offsets.
/// \param [in] begin_bit - [optional] index of the first (least significant) bit used in
/// key comparison. Must be in range <tt>[0; 8 * sizeof(Key))</tt>. Default value: \p 0.
/// Non-default value not supported for floating-point key-types.
/// \param [in] end_bit - [optional] past-the-end index (most significant) bit used in
/// key comparison. Must be in range <tt>(begin_bit; 8 * sizeof(Key)]</tt>. Default
/// value: \p <tt>8 * sizeof(Key)</tt>. Non-default value not supported for floating-point key-types.
/// \param [in] stream - [optional] HIP stream object. Default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. Default value is \p false.
///
/// \returns \p hipSuccess (\p 0) after successful sort; otherwise a HIP runtime error of
/// type \p hipError_t.
///
/// \par Example
/// \parblock
/// In this example a device-level descending radix sort is performed on an array of
/// integer values.
///
/// \code{.cpp}
/// #include <rocprim/rocprim.hpp>
///
/// // Prepare input and tmp (declare pointers, allocate device memory etc.)
/// size_t input_size;       // e.g., 8
/// int * input;             // e.g., [6, 3, 5, 4, 2, 8, 1, 7]
/// int * tmp;               // empty array of 8 elements
/// unsigned int segments;   // e.g., 3
/// int * offsets;           // e.g. [0, 2, 3, 8]
/// // Create double-buffer
/// rocprim::double_buffer<int> keys(input, tmp);
///
/// size_t temporary_storage_size_bytes;
/// void * temporary_storage_ptr = nullptr;
/// // Get required size of the temporary storage
/// rocprim::segmented_radix_sort_keys_desc(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys, input_size,
///     segments, offsets, offsets + 1
/// );
///
/// // allocate temporary storage
/// hipMalloc(&temporary_storage_ptr, temporary_storage_size_bytes);
///
/// // perform sort
/// rocprim::segmented_radix_sort_keys_desc(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys, input_size,
///     segments, offsets, offsets + 1
/// );
/// // keys.current(): [6, 3, 5, 8, 7, 4, 2, 1]
/// \endcode
/// \endparblock
template<
    class Config = default_config,
    class Key,
    class OffsetIterator
>
inline
hipError_t segmented_radix_sort_keys_desc(void * temporary_storage,
                                          size_t& storage_size,
                                          double_buffer<Key>& keys,
                                          unsigned int size,
                                          unsigned int segments,
                                          OffsetIterator begin_offsets,
                                          OffsetIterator end_offsets,
                                          unsigned int begin_bit = 0,
                                          unsigned int end_bit = 8 * sizeof(Key),
                                          hipStream_t stream = 0,
                                          bool debug_synchronous = false)
{
    empty_type * values = nullptr;
    bool is_result_in_output;
    hipError_t error = detail::segmented_radix_sort_impl<Config, true>(
        temporary_storage, storage_size,
        keys.current(), keys.current(), keys.alternate(),
        values, values, values,
        size, is_result_in_output,
        segments, begin_offsets, end_offsets,
        begin_bit, end_bit,
        stream, debug_synchronous
    );
    if(temporary_storage != nullptr && is_result_in_output)
    {
        keys.swap();
    }
    return error;
}

/// \brief Parallel ascending radix sort-by-key primitive for device level.
///
/// \p segmented_radix_sort_pairs_desc function performs a device-wide radix sort across multiple,
/// non-overlapping sequences of (key, value) pairs. Function sorts input pairs in ascending order of keys.
///
/// \par Overview
/// * The contents of both buffers of \p keys and \p values may be altered by the sorting function.
/// * \p current() of \p keys and \p values are used as the input.
/// * The function will update \p current() of \p keys and \p values to point to buffers
/// that contains the output range.
/// * Returns the required size of \p temporary_storage in \p storage_size
/// if \p temporary_storage in a null pointer.
/// * The function requires small \p temporary_storage as it does not need
/// a temporary buffer of \p size elements.
/// * \p Key type must be an arithmetic type (that is, an integral type or a floating-point
/// type).
/// * Buffers of \p keys must have at least \p size elements.
/// * Ranges specified by \p begin_offsets and \p end_offsets must have
/// at least \p segments elements. They may use the same sequence <tt>offsets</tt> of at least
/// <tt>segments + 1</tt> elements: <tt>offsets</tt> for \p begin_offsets and
/// <tt>offsets + 1</tt> for \p end_offsets.
/// * If \p Key is an integer type and the range of keys is known in advance, the performance
/// can be improved by setting \p begin_bit and \p end_bit, for example if all keys are in range
/// [100, 10000], <tt>begin_bit = 0</tt> and <tt>end_bit = 14</tt> will cover the whole range.
///
/// \tparam Config - [optional] configuration of the primitive. It can be
/// \p segmented_radix_sort_config or a custom class with the same members.
/// \tparam Key - key type. Must be an integral type or a floating-point type.
/// \tparam Value - value type.
/// \tparam OffsetIterator - random-access iterator type of segment offsets. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
///
/// \param [in] temporary_storage - pointer to a device-accessible temporary storage. When
/// a null pointer is passed, the required allocation size (in bytes) is written to
/// \p storage_size and function returns without performing the sort operation.
/// \param [in,out] storage_size - reference to a size (in bytes) of \p temporary_storage.
/// \param [in,out] keys - reference to the double-buffer of keys, its \p current()
/// contains the input range and will be updated to point to the output range.
/// \param [in,out] values - reference to the double-buffer of values, its \p current()
/// contains the input range and will be updated to point to the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] segments - number of segments in the input range.
/// \param [in] begin_offsets - iterator to the first element in the range of beginning offsets.
/// \param [in] end_offsets - iterator to the first element in the range of ending offsets.
/// \param [in] begin_bit - [optional] index of the first (least significant) bit used in
/// key comparison. Must be in range <tt>[0; 8 * sizeof(Key))</tt>. Default value: \p 0.
/// Non-default value not supported for floating-point key-types.
/// \param [in] end_bit - [optional] past-the-end index (most significant) bit used in
/// key comparison. Must be in range <tt>(begin_bit; 8 * sizeof(Key)]</tt>. Default
/// value: \p <tt>8 * sizeof(Key)</tt>. Non-default value not supported for floating-point key-types.
/// \param [in] stream - [optional] HIP stream object. Default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. Default value is \p false.
///
/// \returns \p hipSuccess (\p 0) after successful sort; otherwise a HIP runtime error of
/// type \p hipError_t.
///
/// \par Example
/// \parblock
/// In this example a device-level ascending radix sort is performed where input keys are
/// represented by an array of unsigned integers and input values by an array of <tt>double</tt>s.
///
/// \code{.cpp}
/// #include <rocprim/rocprim.hpp>
///
/// // Prepare input and tmp (declare pointers, allocate device memory etc.)
/// size_t input_size;          // e.g., 8
/// unsigned int * keys_input;  // e.g., [ 6, 3,  5, 4,  1,  8,  1, 7]
/// double * values_input;      // e.g., [-5, 2, -4, 3, -1, -8, -2, 7]
/// unsigned int * keys_tmp;    // empty array of 8 elements
/// double*  values_tmp;        // empty array of 8 elements
/// unsigned int segments;      // e.g., 3
/// int * offsets;              // e.g. [0, 2, 3, 8]
/// // Create double-buffers
/// rocprim::double_buffer<unsigned int> keys(keys_input, keys_tmp);
/// rocprim::double_buffer<double> values(values_input, values_tmp);
///
/// // Keys are in range [0; 8], so we can limit compared bit to bits on indexes
/// // 0, 1, 2, 3, and 4. In order to do this begin_bit is set to 0 and end_bit
/// // is set to 5.
///
/// size_t temporary_storage_size_bytes;
/// void * temporary_storage_ptr = nullptr;
/// // Get required size of the temporary storage
/// rocprim::segmented_radix_sort_pairs(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys, values, input_size,
///     segments, offsets, offsets + 1
///     0, 5
/// );
///
/// // allocate temporary storage
/// hipMalloc(&temporary_storage_ptr, temporary_storage_size_bytes);
///
/// // perform sort
/// rocprim::segmented_radix_sort_pairs(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys, values, input_size,
///     segments, offsets, offsets + 1
///     0, 5
/// );
/// // keys.current():   [3,  6,  5,  1,  1, 4, 7,  8]
/// // values.current(): [2, -5, -4, -1, -2, 3, 7, -8]
/// \endcode
/// \endparblock
template<
    class Config = default_config,
    class Key,
    class Value,
    class OffsetIterator
>
inline
hipError_t segmented_radix_sort_pairs(void * temporary_storage,
                                      size_t& storage_size,
                                      double_buffer<Key>& keys,
                                      double_buffer<Value>& values,
                                      unsigned int size,
                                      unsigned int segments,
                                      OffsetIterator begin_offsets,
                                      OffsetIterator end_offsets,
                                      unsigned int begin_bit = 0,
                                      unsigned int end_bit = 8 * sizeof(Key),
                                      hipStream_t stream = 0,
                                      bool debug_synchronous = false)
{
    bool is_result_in_output;
    hipError_t error = detail::segmented_radix_sort_impl<Config, false>(
        temporary_storage, storage_size,
        keys.current(), keys.current(), keys.alternate(),
        values.current(), values.current(), values.alternate(),
        size, is_result_in_output,
        segments, begin_offsets, end_offsets,
        begin_bit, end_bit,
        stream, debug_synchronous
    );
    if(temporary_storage != nullptr && is_result_in_output)
    {
        keys.swap();
        values.swap();
    }
    return error;
}

/// \brief Parallel descending radix sort-by-key primitive for device level.
///
/// \p segmented_radix_sort_pairs_desc function performs a device-wide radix sort across multiple,
/// non-overlapping sequences of (key, value) pairs. Function sorts input pairs in descending order of keys.
///
/// \par Overview
/// * The contents of both buffers of \p keys and \p values may be altered by the sorting function.
/// * \p current() of \p keys and \p values are used as the input.
/// * The function will update \p current() of \p keys and \p values to point to buffers
/// that contains the output range.
/// * Returns the required size of \p temporary_storage in \p storage_size
/// if \p temporary_storage in a null pointer.
/// * The function requires small \p temporary_storage as it does not need
/// a temporary buffer of \p size elements.
/// * \p Key type must be an arithmetic type (that is, an integral type or a floating-point
/// type).
/// * Buffers of \p keys must have at least \p size elements.
/// * Ranges specified by \p begin_offsets and \p end_offsets must have
/// at least \p segments elements. They may use the same sequence <tt>offsets</tt> of at least
/// <tt>segments + 1</tt> elements: <tt>offsets</tt> for \p begin_offsets and
/// <tt>offsets + 1</tt> for \p end_offsets.
/// * If \p Key is an integer type and the range of keys is known in advance, the performance
/// can be improved by setting \p begin_bit and \p end_bit, for example if all keys are in range
/// [100, 10000], <tt>begin_bit = 0</tt> and <tt>end_bit = 14</tt> will cover the whole range.
///
/// \tparam Config - [optional] configuration of the primitive. It can be
/// \p segmented_radix_sort_config or a custom class with the same members.
/// \tparam Key - key type. Must be an integral type or a floating-point type.
/// \tparam Value - value type.
/// \tparam OffsetIterator - random-access iterator type of segment offsets. Must meet the
/// requirements of a C++ OutputIterator concept. It can be a simple pointer type.
///
/// \param [in] temporary_storage - pointer to a device-accessible temporary storage. When
/// a null pointer is passed, the required allocation size (in bytes) is written to
/// \p storage_size and function returns without performing the sort operation.
/// \param [in,out] storage_size - reference to a size (in bytes) of \p temporary_storage.
/// \param [in,out] keys - reference to the double-buffer of keys, its \p current()
/// contains the input range and will be updated to point to the output range.
/// \param [in,out] values - reference to the double-buffer of values, its \p current()
/// contains the input range and will be updated to point to the output range.
/// \param [in] size - number of element in the input range.
/// \param [in] segments - number of segments in the input range.
/// \param [in] begin_offsets - iterator to the first element in the range of beginning offsets.
/// \param [in] end_offsets - iterator to the first element in the range of ending offsets.
/// \param [in] begin_bit - [optional] index of the first (least significant) bit used in
/// key comparison. Must be in range <tt>[0; 8 * sizeof(Key))</tt>. Default value: \p 0.
/// Non-default value not supported for floating-point key-types.
/// \param [in] end_bit - [optional] past-the-end index (most significant) bit used in
/// key comparison. Must be in range <tt>(begin_bit; 8 * sizeof(Key)]</tt>. Default
/// value: \p <tt>8 * sizeof(Key)</tt>. Non-default value not supported for floating-point key-types.
/// \param [in] stream - [optional] HIP stream object. Default is \p 0 (default stream).
/// \param [in] debug_synchronous - [optional] If true, synchronization after every kernel
/// launch is forced in order to check for errors. Default value is \p false.
///
/// \returns \p hipSuccess (\p 0) after successful sort; otherwise a HIP runtime error of
/// type \p hipError_t.
///
/// \par Example
/// \parblock
/// In this example a device-level descending radix sort is performed where input keys are
/// represented by an array of integers and input values by an array of <tt>double</tt>s.
///
/// \code{.cpp}
/// #include <rocprim/rocprim.hpp>
///
/// // Prepare input and tmp (declare pointers, allocate device memory etc.)
/// size_t input_size;       // e.g., 8
/// int * keys_input;        // e.g., [ 6, 3,  5, 4,  1,  8,  1, 7]
/// double * values_input;   // e.g., [-5, 2, -4, 3, -1, -8, -2, 7]
/// int * keys_tmp;          // empty array of 8 elements
/// double * values_tmp;     // empty array of 8 elements
/// unsigned int segments;   // e.g., 3
/// int * offsets;           // e.g. [0, 2, 3, 8]
/// // Create double-buffers
/// rocprim::double_buffer<int> keys(keys_input, keys_tmp);
/// rocprim::double_buffer<double> values(values_input, values_tmp);
///
/// size_t temporary_storage_size_bytes;
/// void * temporary_storage_ptr = nullptr;
/// // Get required size of the temporary storage
/// rocprim::segmented_radix_sort_pairs_desc(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys, values, input_size,
///     segments, offsets, offsets + 1
/// );
///
/// // allocate temporary storage
/// hipMalloc(&temporary_storage_ptr, temporary_storage_size_bytes);
///
/// // perform sort
/// rocprim::segmented_radix_sort_pairs_desc(
///     temporary_storage_ptr, temporary_storage_size_bytes,
///     keys, values, input_size,
///     segments, offsets, offsets + 1
/// );
/// // keys.current():   [ 6, 3,  5,  8, 7, 4,  1,  1]
/// // values.current(): [-5, 2, -4, -8, 7, 3, -1, -2]
/// \endcode
/// \endparblock
template<
    class Config = default_config,
    class Key,
    class Value,
    class OffsetIterator
>
inline
hipError_t segmented_radix_sort_pairs_desc(void * temporary_storage,
                                           size_t& storage_size,
                                           double_buffer<Key>& keys,
                                           double_buffer<Value>& values,
                                           unsigned int size,
                                           unsigned int segments,
                                           OffsetIterator begin_offsets,
                                           OffsetIterator end_offsets,
                                           unsigned int begin_bit = 0,
                                           unsigned int end_bit = 8 * sizeof(Key),
                                           hipStream_t stream = 0,
                                           bool debug_synchronous = false)
{
    bool is_result_in_output;
    hipError_t error = detail::segmented_radix_sort_impl<Config, true>(
        temporary_storage, storage_size,
        keys.current(), keys.current(), keys.alternate(),
        values.current(), values.current(), values.alternate(),
        size, is_result_in_output,
        segments, begin_offsets, end_offsets,
        begin_bit, end_bit,
        stream, debug_synchronous
    );
    if(temporary_storage != nullptr && is_result_in_output)
    {
        keys.swap();
        values.swap();
    }
    return error;
}

END_ROCPRIM_NAMESPACE

/// @}
// end of group devicemodule

#endif // ROCPRIM_DEVICE_DEVICE_SEGMENTED_RADIX_SORT_HPP_
