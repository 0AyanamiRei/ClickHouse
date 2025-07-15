#include <Processors/Formats/Impl/ParquetV3BlockInputFormat.h>

#if USE_PARQUET

#include <Common/ThreadPool.h>
#include <Processors/Formats/Impl/Parquet/SchemaConverter.h>
#include <Formats/FormatParserGroup.h>
#include <IO/SharedThreadPools.h>

namespace DB
{

Parquet::ReadOptions convertReadOptions(const FormatSettings & format_settings)
{
    Parquet::ReadOptions options;
    options.format = format_settings;

    options.schema_inference_force_nullable = format_settings.schema_inference_make_columns_nullable == 1;
    options.schema_inference_force_not_nullable = format_settings.schema_inference_make_columns_nullable == 0;

    return options;
}

ParquetV3BlockInputFormat::ParquetV3BlockInputFormat(
    ReadBuffer & buf,
    const Block & header_,
    const FormatSettings & format_settings_,
    FormatParserGroupPtr parser_group_,
    size_t min_bytes_for_seek)
    : IInputFormat(header_, &buf)
    , format_settings(format_settings_)
    , read_options(convertReadOptions(format_settings))
    , parser_group(parser_group_)
{
    read_options.min_bytes_for_seek = min_bytes_for_seek;
    read_options.bytes_per_read_task = min_bytes_for_seek * 4;
}

void ParquetV3BlockInputFormat::initializeIfNeeded()
{
    if (!reader)
    {
        parser_group->initOnce([&]
            {
                parser_group->initKeyCondition(getPort().getHeader());

                if (format_settings.parquet.enable_row_group_prefetch && parser_group->max_io_threads > 0)
                    parser_group->io_runner.initThreadPool(
                        getFormatParsingThreadPool().get(), parser_group->max_io_threads, "ParquetPrefetch", CurrentThread::getGroup());

                /// Unfortunately max_parsing_threads setting doesn't have a value for
                /// "do parsing in the same thread as the rest of query processing
                /// (inside IInputFormat::read()), with no thread pool". But such mode seems
                /// useful, at least for testing performance. So we use max_parsing_threads = 1
                /// as a signal to disable thread pool altogether, sacrificing the ability to
                /// use thread pool with 1 thread. We could subtract 1 instead, but then the
                /// by default the thread pool will use `num_cores - 1` threads, also bad.
                if (parser_group->max_parsing_threads <= 1 || format_settings.parquet.preserve_order)
                    parser_group->parsing_runner.initManual();
                else
                    parser_group->parsing_runner.initThreadPool(
                        getFormatParsingThreadPool().get(), parser_group->max_parsing_threads, "ParquetDecoder", CurrentThread::getGroup());

                auto ext = std::make_shared<Parquet::ParserGroupExt>();

                if (parser_group->key_condition)
                    parser_group->key_condition->extractSingleColumnConditions(ext->column_conditions, nullptr);

                ext->total_memory_low_watermark = format_settings.parquet.memory_low_watermark;
                ext->total_memory_high_watermark = format_settings.parquet.memory_high_watermark;
                parser_group->opaque = ext;
            });

        reader.emplace();
        reader->reader.prefetcher.init(in, read_options, parser_group);
        reader->reader.init(read_options, getPort().getHeader(), parser_group);
        reader->init(parser_group);
    }
}

Chunk ParquetV3BlockInputFormat::read()
{
    initializeIfNeeded();
    Chunk chunk;
    std::tie(chunk, previous_block_missing_values) = reader->read();
    return chunk;
}

const BlockMissingValues * ParquetV3BlockInputFormat::getMissingValues() const
{
    return &previous_block_missing_values;
}

void ParquetV3BlockInputFormat::onCancel() noexcept
{
    if (reader)
        reader->cancel();
}

void ParquetV3BlockInputFormat::resetParser()
{
    reader.reset();
    previous_block_missing_values.clear();
    IInputFormat::resetParser();
}

NativeParquetSchemaReader::NativeParquetSchemaReader(ReadBuffer & in_, const FormatSettings & format_settings)
    : ISchemaReader(in_)
    , read_options(convertReadOptions(format_settings))
{
}

void NativeParquetSchemaReader::initializeIfNeeded()
{
    if (initialized)
        return;
    Parquet::Prefetcher prefetcher;
    prefetcher.init(&in, read_options, /*parser_group_=*/ nullptr);
    file_metadata = Parquet::Reader::readFileMetaData(prefetcher);
    initialized = true;
}

NamesAndTypesList NativeParquetSchemaReader::readSchema()
{
    initializeIfNeeded();
    Parquet::SchemaConverter schemer(file_metadata, read_options, /*sample_block*/ nullptr);
    return schemer.inferSchema();
}

std::optional<size_t> NativeParquetSchemaReader::readNumberOrRows()
{
    initializeIfNeeded();
    return size_t(file_metadata.num_rows);
}

}

#endif
