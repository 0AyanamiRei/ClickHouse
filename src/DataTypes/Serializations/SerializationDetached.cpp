#include <DataTypes/Serializations/SerializationDetached.h>

#include <Columns/ColumnBlob.h>
#include <Columns/IColumn.h>
#include <Compression/CompressedWriteBuffer.h>
#include <DataTypes/Serializations/ISerialization.h>
#include <IO/VarInt.h>
#include <Common/Exception.h>
#include <Common/typeid_cast.h>

namespace DB
{
namespace ErrorCodes
{
extern const int LOGICAL_ERROR;
}

SerializationDetached::SerializationDetached(const SerializationPtr & nested_) : nested(nested_)
{
}

void SerializationDetached::serializeBinaryBulk(
    const IColumn & column, WriteBuffer & ostr, [[maybe_unused]] size_t offset, [[maybe_unused]] size_t limit) const
{
    // We will write directly into the uncompressed buffer. It also means we don't need to flush the data written by us here.
    WriteBuffer * uncompressed_buf = &ostr;
    if (auto * compressed_buf = typeid_cast<CompressedWriteBuffer *>(&ostr))
    {
        // Flush all pending data
        ostr.next();
        if (ostr.offset() != 0)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "SerializationDetached: ostr.offset() is not zero");
        uncompressed_buf = &compressed_buf->getImplBuffer();
    }

    const auto & blob = typeid_cast<const ColumnBlob &>(column).getBlob();
    writeVarUInt(blob.size(), *uncompressed_buf);
    (*uncompressed_buf).write(blob.data(), blob.size());
}

void SerializationDetached::deserializeBinaryBulk(
    IColumn & column,
    ReadBuffer & istr,
    [[maybe_unused]] size_t rows_offset,
    [[maybe_unused]] size_t limit,
    [[maybe_unused]] double avg_value_size_hint) const
{
    // We will read directly from the uncompressed buffer.
    ReadBuffer * uncompressed_buf = &istr;
    if (auto * compressed_buf = typeid_cast<CompressedReadBuffer *>(&istr))
    {
        // Each compressed block is prefixed with the its size, so we know that the compressed buffer should be drained at this point.
        if (istr.available() != 0)
            throw Exception(ErrorCodes::LOGICAL_ERROR, "SerializationDetached: istr.available() is not zero");
        uncompressed_buf = &compressed_buf->getImplBuffer();
    }

    auto & blob = typeid_cast<ColumnBlob &>(column).getBlob();
    size_t bytes = 0;
    readVarUInt(bytes, *uncompressed_buf);
    blob.resize(bytes);
    (*uncompressed_buf).readStrict(blob.data(), blob.size());
}

void SerializationDetached::deserializeBinaryBulkWithMultipleStreams(
    ColumnPtr & column,
    size_t rows_offset,
    size_t limit,
    DeserializeBinaryBulkSettings & settings,
    DeserializeBinaryBulkStatePtr & state,
    SubstreamsCache * cache) const
{
    ColumnPtr concrete_column(column->cloneEmpty());
    auto task = [concrete_column,
                 nested_serialization = nested,
                 limit,
                 format_settings = settings.format_settings,
                 avg_value_size_hint = settings.avg_value_size_hint](const ColumnBlob::Blob & blob, int)
    {
        // In case of alias columns, `column` might be a reference to the same column for a number of calls to this function.
        // To avoid deserializing into the same column multiple times, we clone the column here one more time.
        return ColumnBlob::fromBlob(blob, concrete_column->cloneEmpty(), nested_serialization, limit, format_settings, avg_value_size_hint);
    };

    auto column_blob = ColumnPtr(ColumnBlob::create(std::move(task), concrete_column, limit));
    ISerialization::deserializeBinaryBulkWithMultipleStreams(column_blob, rows_offset, limit, settings, state, cache);
    column = column_blob;
}

[[noreturn]] void SerializationDetached::throwInapplicable()
{
    throw Exception(ErrorCodes::LOGICAL_ERROR, "ColumnBlob should be converted to a regular column before usage");
}
}
