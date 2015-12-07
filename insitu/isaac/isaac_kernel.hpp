/* This file is part of ISAAC.
 *
 * ISAAC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * ISAAC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Lesser Public
 * License along with ISAAC.  If not, see <www.gnu.org/licenses/>. */

#pragma once

#include "isaac_macros.hpp"
#include "isaac_fusion_extension.hpp"

namespace isaac
{

namespace fus = boost::fusion;
namespace mpl = boost::mpl;

ISAAC_NO_HOST_DEVICE_WARNING
template <int N>
ISAAC_HOST_DEVICE_INLINE isaac_float_dim<1> isaacLength( isaac_float_dim<N> );

template <>
ISAAC_HOST_DEVICE_INLINE isaac_float_dim<1> isaacLength<4>( isaac_float_dim<4> v )
{
    isaac_float_dim<1> result;
    result.value.x = sqrt(
        v.value.x * v.value.x +
        v.value.y * v.value.y +
        v.value.z * v.value.z +
        v.value.w * v.value.w
    );
    return result;
}

template <>
ISAAC_HOST_DEVICE_INLINE isaac_float_dim<1> isaacLength<3>( isaac_float_dim<3> v )
{
    isaac_float_dim<1> result;
    result.value.x = sqrt(
        v.value.x * v.value.x +
        v.value.y * v.value.y +
        v.value.z * v.value.z
    );
    return result;
}

template <>
ISAAC_HOST_DEVICE_INLINE isaac_float_dim<1> isaacLength<2>( isaac_float_dim<2> v )
{
    isaac_float_dim<1> result;
    result.value.x = sqrt(
        v.value.x * v.value.x +
        v.value.y * v.value.y
    );
    return result;
}

template <>
ISAAC_HOST_DEVICE_INLINE isaac_float_dim<1> isaacLength<1>( isaac_float_dim<1> v )
{
    isaac_float_dim<1> result;
    result.value.x = fabs(v.value.x);
    return result;
}

struct IsaacChainLength
{
    ISAAC_NO_HOST_DEVICE_WARNING
    template <int N>
    ISAAC_HOST_DEVICE_INLINE static isaac_float_dim<1> call( isaac_float_dim<N> v )
    {
        return isaacLength<N>( v );
    }
};

template < size_t Ttransfer_size, isaac_int Tinterpolation >
struct merge_source_iterator
{
    ISAAC_NO_HOST_DEVICE_WARNING
    template
    <
        typename TSource,
        typename TColor,
        typename TPos,
        typename TLocalSize,
        typename TTransferArray
    >
    ISAAC_HOST_DEVICE_INLINE  void operator()(
        const int I,
        TSource& source,
        TColor& color,
        TPos& pos,
        TLocalSize& local_size,
        TTransferArray& transferArray) const
    {
        bool error = false;
        isaac_float_dim < TSource::feature_dim > data;
        if (Tinterpolation == 0)
        {
            isaac_uint3 coord =
            {
                isaac_uint(round(pos.x)),
                isaac_uint(round(pos.y)),
                isaac_uint(round(pos.z))
            };
            if ( ISAAC_FOR_EACH_DIM_TWICE(3, coord, >= local_size.value, || ) 0 )
                error = true;
            else
                data = source[coord];
        }
        else
        {
            isaac_uint3 coord;
            isaac_float_dim < TSource::feature_dim > data8[2][2][2];
            for (int x = 0; x < 2; x++)
                for (int y = 0; y < 2; y++)
                    for (int z = 0; z < 2; z++)
                    {
                        coord.x = isaac_uint(x?ceil(pos.x):floor(pos.x));
                        coord.y = isaac_uint(y?ceil(pos.y):floor(pos.y));
                        coord.z = isaac_uint(z?ceil(pos.z):floor(pos.z));
                        if ( coord.x >= local_size.value.x )
                        {
                            coord.x = isaac_uint(x?floor(pos.x):ceil(pos.x));
                            if ( coord.x >= local_size.value.x )
                            {
                                error = true;
                                break;
                            }
                        }
                        if ( coord.y >= local_size.value.y )
                        {
                            coord.y = isaac_uint(y?floor(pos.y):ceil(pos.y));
                            if ( coord.y >= local_size.value.y )
                            {
                                error = true;
                                break;
                            }
                        }
                        if ( coord.z >= local_size.value.z )
                        {
                            coord.z = isaac_uint(z?floor(pos.z):ceil(pos.z));
                            if ( coord.z >= local_size.value.z )
                            {
                                error = true;
                                break;
                            }
                        }
                        data8[x][y][z] = source[coord];
                    }
            if (!error)
            {
                isaac_float_dim < 3 > pos_in_cube =
                {
                    pos.x - floor(pos.x),
                    pos.y - floor(pos.y),
                    pos.z - floor(pos.z)
                };
                isaac_float_dim < TSource::feature_dim > data4[2][2];
                for (int x = 0; x < 2; x++)
                    for (int y = 0; y < 2; y++)
                        data4[x][y].value =
                            data8[x][y][0].value * (isaac_float(1) - pos_in_cube.value.z) +
                            data8[x][y][1].value * (                 pos_in_cube.value.z);
                isaac_float_dim < TSource::feature_dim > data2[2];
                for (int x = 0; x < 2; x++)
                    data2[x].value =
                        data4[x][0].value * (isaac_float(1) - pos_in_cube.value.y) +
                        data4[x][1].value * (                 pos_in_cube.value.y);
                data.value =
                    data2[0].value * (isaac_float(1) - pos_in_cube.value.x) +
                    data2[1].value * (                 pos_in_cube.value.x);
            }
        }
        if (!error)
        {
            isaac_float_dim<1> result = IsaacChainLength::call< TSource::feature_dim >( data );
            isaac_uint lookup_value = isaac_uint( round(result.value.x * isaac_float( Ttransfer_size ) ) );
            if (lookup_value >= Ttransfer_size )
                lookup_value = Ttransfer_size - 1;
            isaac_float4 value = transferArray.pointer[ I ][ lookup_value ];
            color.x = color.x + value.x * value.w;
            color.y = color.y + value.y * value.w;
            color.z = color.z + value.z * value.w;
            color.w = color.w + value.w;
        }
    }
};


#if ISAAC_ALPAKA == 0
    __constant__ isaac_float isaac_inverse_d[16];
    __constant__ isaac_size_struct<3> isaac_size_d[1]; //[1] to access it same for cuda and alpaka the same way
#endif

template <
    typename TSimDim,
    typename TSourceList,
    typename TChainList,
    typename TTransferArray,
    size_t Ttransfer_size,
    isaac_int Tinterpolation
>
#if ISAAC_ALPAKA == 1
    struct IsaacFillRectKernel
    {
        template <typename TAcc__>
        ALPAKA_FN_ACC void operator()(
            TAcc__ const &acc,
            isaac_float* isaac_inverse_d,
            isaac_size_struct<TSimDim::value>* isaac_size_d,
#else
        __global__ void IsaacFillRectKernel(
#endif
            uint32_t* pixels,
            isaac_size2 framebuffer_size,
            isaac_uint2 framebuffer_start,
            TSourceList sources,
            isaac_float step,
            isaac_float4 background_color,
            TTransferArray transferArray )
#if ISAAC_ALPAKA == 1
        const
#endif
        {
            #if ISAAC_ALPAKA == 1
                auto threadIdx = alpaka::idx::getIdx<alpaka::Grid, alpaka::Threads>(acc);
                isaac_uint2 pixel =
                {
                    isaac_uint(threadIdx[2]),
                    isaac_uint(threadIdx[1])
                };
            #else
                isaac_uint2 pixel =
                {
                    isaac_uint(threadIdx.x + blockIdx.x * blockDim.x),
                    isaac_uint(threadIdx.y + blockIdx.y * blockDim.y)
                };
            #endif
            pixel = pixel + framebuffer_start;
            if ( ISAAC_FOR_EACH_DIM_TWICE(2, pixel, >= framebuffer_size, || ) 0 )
                return;
            
            isaac_float2 pixel_f =
            {
                isaac_float( pixel.x )/(isaac_float)framebuffer_size.x*isaac_float(2)-isaac_float(1),
                isaac_float( pixel.y )/(isaac_float)framebuffer_size.y*isaac_float(2)-isaac_float(1)
            };
            isaac_float4 start_p = {pixel_f.x*ISAAC_Z_NEAR,pixel_f.y*ISAAC_Z_NEAR,-1.0f*ISAAC_Z_NEAR,1.0f*ISAAC_Z_NEAR}; //znear
            isaac_float4   end_p = {pixel_f.x* ISAAC_Z_FAR,pixel_f.y* ISAAC_Z_FAR, 1.0f* ISAAC_Z_FAR,1.0f* ISAAC_Z_FAR}; //zfar
            isaac_float3 start =
            {
                isaac_inverse_d[ 0] * start_p.x + isaac_inverse_d[ 4] * start_p.y +  isaac_inverse_d[ 8] * start_p.z + isaac_inverse_d[12] * start_p.w,
                isaac_inverse_d[ 1] * start_p.x + isaac_inverse_d[ 5] * start_p.y +  isaac_inverse_d[ 9] * start_p.z + isaac_inverse_d[13] * start_p.w,
                isaac_inverse_d[ 2] * start_p.x + isaac_inverse_d[ 6] * start_p.y +  isaac_inverse_d[10] * start_p.z + isaac_inverse_d[14] * start_p.w
            };
            isaac_float3 end =
            {
                isaac_inverse_d[ 0] *   end_p.x + isaac_inverse_d[ 4] *   end_p.y +  isaac_inverse_d[ 8] *   end_p.z + isaac_inverse_d[12] *   end_p.w,
                isaac_inverse_d[ 1] *   end_p.x + isaac_inverse_d[ 5] *   end_p.y +  isaac_inverse_d[ 9] *   end_p.z + isaac_inverse_d[13] *   end_p.w,
                isaac_inverse_d[ 2] *   end_p.x + isaac_inverse_d[ 6] *   end_p.y +  isaac_inverse_d[10] *   end_p.z + isaac_inverse_d[14] *   end_p.w
            };
            isaac_float max_size = isaac_size_d[0].max_global_size / 2.0f;

            //scale to globale grid size
            start = start * max_size;
              end =   end * max_size;

            //move to local grid
            isaac_int3 move =
            {
                isaac_int(isaac_size_d[0].global_size.value.x) / isaac_int(2) - isaac_int(isaac_size_d[0].position.value.x),
                isaac_int(isaac_size_d[0].global_size.value.y) / isaac_int(2) - isaac_int(isaac_size_d[0].position.value.y),
                isaac_int(isaac_size_d[0].global_size.value.z) / isaac_int(2) - isaac_int(isaac_size_d[0].position.value.z)
            };
            isaac_float3 move_f =
            {
                isaac_float(move.x),
                isaac_float(move.y),
                isaac_float(move.z)
            };
            start = start + move_f;
              end =   end + move_f;

            isaac_float3 vec = end - start;
            isaac_float l = sqrt( vec.x * vec.x + vec.y * vec.y + vec.z * vec.z );
            
            isaac_float3 step_vec = vec / l * step;
            isaac_float3 count_start =  - start / step_vec;
            isaac_float3 local_size_f =
            {
                isaac_float(isaac_size_d[0].local_size.value.x),
                isaac_float(isaac_size_d[0].local_size.value.y),
                isaac_float(isaac_size_d[0].local_size.value.z)
            };
            isaac_float3 count_end = ( local_size_f - start ) / step_vec;

            //count_start shall have the smaller values
            ISAAC_SWITCH_IF_SMALLER( count_end.x, count_start.x )
            ISAAC_SWITCH_IF_SMALLER( count_end.y, count_start.y )
            ISAAC_SWITCH_IF_SMALLER( count_end.z, count_start.z )
            
            //calc intersection of all three super planes and save in [count_start.x ; count_end.x]
            count_start.x = max( max( count_start.x, count_start.y ), count_start.z );
              count_end.x = min( min(   count_end.x,   count_end.y ),   count_end.z );
            if ( count_start.x > count_end.x || count_end.x <= 0.0f )
            {
                ISAAC_SET_COLOR( pixels[pixel.x + pixel.y * framebuffer_size.x], background_color )
                return;
            }
            
            //Starting the main loop
            isaac_int first = floor( count_start.x );
            isaac_int last = ceil( count_end.x );

            isaac_int count = last - first + 1;
            isaac_float4 color = {isaac_float(0),isaac_float(0),isaac_float(0),isaac_float(0)};
            isaac_float3 pos = start + step_vec * isaac_float(first);
            isaac_float factor = isaac_float(2) / isaac_size_d[0].max_global_size;
            for (isaac_int i = 0; i < count; i++)
            {
                isaac_float4 value = {0, 0, 0, 0};
                isaac_for_each_4_params( sources, merge_source_iterator<Ttransfer_size,Tinterpolation>() , value, pos, isaac_size_d[0].local_size, transferArray);
                if ( mpl::size< TSourceList >::type::value > 1)
                    value = value / isaac_float( mpl::size< TSourceList >::type::value );
                isaac_float oma = isaac_float(1) - color.w;
                value = value * factor;
                isaac_float4 color_add =
                {
                    oma * value.x, // * value.w does merge_source_iterator
                    oma * value.y, // * value.w does merge_source_iterator
                    oma * value.z, // * value.w does merge_source_iterator
                    oma * value.w
                };
                color = color + color_add;
                pos = pos + step_vec;
            }
            ISAAC_SET_COLOR( pixels[pixel.x + pixel.y * framebuffer_size.x], color )
        }
#if ISAAC_ALPAKA == 1
    };
#endif

} //namespace isaac;