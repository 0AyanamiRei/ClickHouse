#include <AggregateFunctions/AggregateFunctionFactory.h>
#include <AggregateFunctions/AggregateFunctionQuantile.h>
#include <AggregateFunctions/Helpers.h>
#include <Core/Field.h>
#include <DataTypes/DataTypeDate.h>
#include <DataTypes/DataTypeDateTime.h>
#include <Common/HashTable/HashMap.h>
#include <Common/NaNUtils.h>

#include <numeric>


namespace DB
{

struct Settings;

namespace ErrorCodes
{
    extern const int NUMBER_OF_ARGUMENTS_DOESNT_MATCH;
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
}

namespace
{

template <typename Value>
struct QuantileHistogram
{
    using CumulativeHistogramValue = Float64;
    using UnderlyingType = NativeType<Value>;
    using Hasher = HashCRC32<UnderlyingType>;

    /// When creating, the hash table must be small.
    using Map = HashMapWithStackMemory<UnderlyingType, CumulativeHistogramValue, Hasher, 4>;
    using Pair = typename Map::value_type;

    Map map;

    void add(const Value & x, CumulativeHistogramValue cumulative_histogram_value)
    {
        if (!isNaN(x))
            map[x] += cumulative_histogram_value;
    }

    void merge(const QuantileHistogram & rhs)
    {
        for (const auto & pair : rhs.map)
            map[pair.getKey()] += pair.getMapped();
    }

    void serialize(WriteBuffer & buf) const
    {
        map.write(buf);
    }

    void deserialize(ReadBuffer & buf)
    {
        typename Map::Reader reader(buf);
        while (reader.next())
        {
            const auto & pair = reader.get();
            map[pair.first] = pair.second;
        }
    }

    /// Get the value of the `level` quantile. The level must be between 0 and 1.
    Value get(Float64 level) const
    {
        size_t size = map.size();
        if (0 == size)
            return Value();

        Float64 res = getFloatInterpolatedImpl(level);
        if constexpr (is_decimal<Value>)
            return Value(static_cast<typename Value::NativeType>(res));
        else
            return static_cast<Value>(res);
    }

    /// Get the `size` values of `levels` quantiles. Write `size` results starting with `result` address.
    /// indices - an array of index levels such that the corresponding elements will go in ascending order.
    void getMany(const Float64 * levels, const size_t * indices, size_t num_levels, Value * result) const
    {
        size_t size = map.size();
        if (0 == size)
        {
            for (size_t i = 0; i < num_levels; ++i)
                result[i] = Value();
            return;
        }

        std::unique_ptr<Float64 []> res_holder(new Float64[num_levels]);
        Float64 * res = res_holder.get();
        getManyFloatInterpolatedImpl(levels, indices, num_levels, res);
        for (size_t i = 0; i < num_levels; ++i)
        {
            if constexpr (is_decimal<Value>)
                result[i] = Value(static_cast<typename Value::NativeType>(res[i]));
            else
                result[i] = Value(res[i]);
        }
    }

    Float64 getFloat(Float64 level) const
    {
        return getFloatInterpolatedImpl(level);
    }

    void getManyFloat(const Float64 * levels, const size_t * indices, size_t num_levels, Float64 * result) const
    {
        getManyFloatInterpolatedImpl(levels, indices, num_levels, result);
    }

private:
    Float64 getFloatInterpolatedImpl(Float64 level) const
    {
        size_t size = map.size();

        if (size < 2)
            return std::numeric_limits<Float64>::quiet_NaN();

        /// Copy the data to a temporary array to get the element you need in order.
        std::unique_ptr<Pair[]> array_holder(new Pair[size]);
        Pair * array = array_holder.get();

        size_t i = 0;
        for (const auto & pair : map)
        {
            array[i] = pair.getValue();
            ++i;
        }

        ::sort(array, array + size, [](const Pair & a, const Pair & b) { return a.first < b.first; });
        Pair max_bucket = array[size - 1];
        if (max_bucket.first != std::numeric_limits<UnderlyingType>::infinity())
            return std::numeric_limits<Float64>::quiet_NaN();
        CumulativeHistogramValue max_position = max_bucket.second;
        Float64 position = max_position * level;
        return quantileInterpolated(array, size, position);
    }

    void getManyFloatInterpolatedImpl(const Float64 * levels, const size_t * indices, size_t num_levels, Float64 * result) const
    {
        size_t size = map.size();
        if (size < 2)
        {
            for (size_t i = 0; i < num_levels; ++i)
                result[i] = std::numeric_limits<Float64>::quiet_NaN();
            return;
        }

        /// Copy the data to a temporary array to get the element you need in order.
        std::unique_ptr<Pair[]> array_holder(new Pair[size]);
        Pair * array = array_holder.get();

        size_t i = 0;
        for (const auto & pair : map)
        {
            array[i] = pair.getValue();
            ++i;
        }

        ::sort(array, array + size, [](const Pair & a, const Pair & b) { return a.first < b.first; });
        Pair max_bucket = array[size - 1];
        CumulativeHistogramValue max_position = max_bucket.second;

        for (size_t j = 0; j < num_levels; ++j)
        {
            if (max_bucket.first != std::numeric_limits<UnderlyingType>::infinity()) 
            {
                result[indices[j]] = std::numeric_limits<Float64>::quiet_NaN();
            }
            else 
            {
                Float64 position = max_position * levels[indices[j]];
                result[indices[j]] = quantileInterpolated(array, size, position);
            }
        }
    }

    /// Calculate quantile, using linear interpolation between the bucket's lower and upper bound
    Float64 NO_SANITIZE_UNDEFINED quantileInterpolated(const Pair * array, size_t size, Float64 position) const
    {
        const auto * upper_bound_it = std::lower_bound(array, array + size, position, [](const Pair & a, Float64 b) { return a.second < b; });
        if (upper_bound_it == array) 
        { 
            if (upper_bound_it->first > 0) 
            {
                // If position is in the first bucket and the first bucket's upper bounds is positive, perform interpolation as if the first bucket's lower bounds is 0.
                return upper_bound_it->first * (position / upper_bound_it->second);
            } 
            else 
            {
                // Otherwise, if the first bucket's upper bounds is non-positive, return the first bucket's upper bound.
                return upper_bound_it->first;
            }
        }
        else if (upper_bound_it >= array + size - 1) 
        {
            // If the position is in the +Inf bucket, return the largest finite bucket's upper bound, which is the second to last bucket's upper bound.
            return (array + size - 2)->first;
        }
        const auto * lower_bound_it = upper_bound_it - 1;

        UnderlyingType histogram_bucket_lower_bound = lower_bound_it->first;
        Float64 histogram_bucket_lower_value = lower_bound_it->second;
        UnderlyingType histogram_bucket_upper_bound = upper_bound_it->first;
        Float64 histogram_bucket_upper_value = upper_bound_it->second;

        // Interpolate between the lower and upper bounds of the bucket that the position is in.
        return histogram_bucket_lower_bound + (histogram_bucket_upper_bound - histogram_bucket_lower_bound) * (position - histogram_bucket_lower_value) / (histogram_bucket_upper_value - histogram_bucket_lower_value);
    }
};

template <typename Value>
using FuncQuantileHistogram = AggregateFunctionQuantile<
    Value,
    QuantileHistogram<Value>,
    NameQuantileHistogram,
    false, 
    true,
    Float64,
    false,
    false>;
template <typename Value>
using FuncQuantilesHistogram = AggregateFunctionQuantile<
    Value,
    QuantileHistogram<Value>,
    NameQuantilesHistogram,
    false,
    true,
    Float64,
    true,
    false>;

template <template <typename> class Function>
AggregateFunctionPtr createAggregateFunctionQuantile(
    const std::string & name, const DataTypes & argument_types, const Array & params, const Settings *)
{
    if (argument_types.empty())
        throw Exception(ErrorCodes::NUMBER_OF_ARGUMENTS_DOESNT_MATCH, "Aggregate function {} requires at least one argument", name);

    const DataTypePtr & argument_type = argument_types[0];
    WhichDataType which(argument_type);

    if (which.idx == TypeIndex::Float32) return std::make_shared<Function<Float32>>(argument_types, params);
    if (which.idx == TypeIndex::Float64) return std::make_shared<Function<Float64>>(argument_types, params);

    throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Illegal type {} of argument for aggregate function {}",
                    argument_type->getName(), name);
}

}

void registerAggregateFunctionsQuantileHistogram(AggregateFunctionFactory & factory)
{
    /// For aggregate functions returning array we cannot return NULL on empty set.
    AggregateFunctionProperties properties = { .returns_default_when_only_null = true };

    factory.registerFunction(NameQuantileHistogram::name, createAggregateFunctionQuantile<FuncQuantileHistogram>);
    factory.registerFunction(
        NameQuantilesHistogram::name, {createAggregateFunctionQuantile<FuncQuantilesHistogram>, properties});
}

}
