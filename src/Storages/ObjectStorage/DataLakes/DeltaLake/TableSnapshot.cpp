#include "config.h"

#if USE_DELTA_KERNEL_RS

#include <Storages/ObjectStorage/DataLakes/DeltaLake/TableSnapshot.h>
#include <Storages/ObjectStorage/DataLakes/DeltaLake/ObjectInfoWithPartitionColumns.h>
#include <Storages/ObjectStorage/DataLakes/DeltaLake/parseFieldFromString.h>
#include <Core/Types.h>
#include <Core/NamesAndTypes.h>
#include <Core/Field.h>
#include <Common/Exception.h>
#include <Common/logger_useful.h>
#include "getSchemaFromSnapshot.h"
#include "KernelUtils.h"

namespace fs = std::filesystem;

namespace DeltaLake
{

class TableSnapshot::Iterator final : public DB::IObjectIterator
{
public:
    Iterator(
        const KernelExternEngine & engine_,
        const KernelSnapshot & snapshot_,
        const std::string & data_prefix_,
        const DB::NamesAndTypesList & schema_,
        const DB::Names & partition_columns_,
        LoggerPtr log_)
        : scan(KernelUtils::unwrapResult(ffi::scan(snapshot_.get(), engine_.get(), /* predicate */{}), "scan"))
        , scan_data_iterator(KernelUtils::unwrapResult(
            ffi::kernel_scan_data_init(engine_.get(), scan.get()),
            "kernel_scan_data_init"))
        , data_prefix(data_prefix_)
        , schema(schema_)
        , partition_columns(partition_columns_)
        , log(log_)
    {
    }

    size_t estimatedKeysCount() override { return 0; } /// TODO

    DB::ObjectInfoPtr next(size_t) override
    {
        if (data_files.empty())
        {
            auto have_scan_data_res = KernelUtils::unwrapResult(
                ffi::kernel_scan_data_next(scan_data_iterator.get(), this, visitData),
                "kernel_scan_data_next");

            if (!have_scan_data_res)
                return nullptr;
        }

        chassert(!data_files.empty());
        auto object = data_files.front();
        data_files.pop_front();
        return object;
    }

    static void visitData(
        void * engine_context,
        ffi::ExclusiveEngineData * engine_data,
        const struct ffi::KernelBoolSlice selection_vec,
        const ffi::CTransforms *transforms)
    {
        ffi::visit_scan_data(engine_data, selection_vec, transforms, engine_context, Iterator::scanCallback);
    }

    static void scanCallback(
        ffi::NullableCvoid engine_context,
        struct ffi::KernelStringSlice path,
        int64_t size,
        const ffi::Stats * stats,
        const ffi::DvInfo * /* dv_info */,
        const struct ffi::CStringMap * partition_map)
    {
        auto * context = static_cast<TableSnapshot::Iterator *>(engine_context);
        std::string full_path = fs::path(context->data_prefix) / KernelUtils::fromDeltaString(path);

        DB::ObjectInfoWithParitionColumns::PartitionColumnsInfo partitions_info;
        for (const auto & partition_column : context->partition_columns)
        {
            auto * raw_value = ffi::get_from_string_map(partition_map, KernelUtils::toDeltaString(partition_column), KernelUtils::allocateString);
            auto value = std::unique_ptr<std::string>(static_cast<std::string *>(raw_value));
            if (value)
            {
                auto name_and_type = context->schema.tryGetByName(partition_column);
                if (!name_and_type)
                {
                    throw DB::Exception(
                        DB::ErrorCodes::LOGICAL_ERROR,
                        "Cannot find column `{}` in schema, there are only columns: `{}`",
                        partition_column, fmt::join(context->schema.getNames(), ", "));
                }
                partitions_info.emplace_back(name_and_type.value(), DB::parseFieldFromString(*value, name_and_type->type));
            }
        }

        DB::ObjectInfoPtr object;
        if (partitions_info.empty())
            object = std::make_shared<DB::ObjectInfo>(full_path, size);
        else
            object = std::make_shared<DB::ObjectInfoWithParitionColumns>(std::move(partitions_info), full_path, size);

        context->data_files.push_back(object);
        LOG_TEST(
            context->log,
            "Scanned file: {}, size: {}, num records: {}, partition columns: {}",
            full_path, size, stats->num_records, partitions_info.size());
    }

private:
    using KernelScan = TemplatedKernelPointerWrapper<ffi::SharedScan, ffi::free_scan>;
    using KernelScanDataIterator = TemplatedKernelPointerWrapper<ffi::SharedScanDataIterator, ffi::free_kernel_scan_data>;

    const KernelScan scan;
    const KernelScanDataIterator scan_data_iterator;
    const std::string data_prefix;
    const DB::NamesAndTypesList & schema;
    const DB::Names & partition_columns;
    const LoggerPtr log;

    std::deque<DB::ObjectInfoPtr> data_files;
};


TableSnapshot::TableSnapshot(KernelHelperPtr helper_, LoggerPtr log_)
    : helper(helper_)
    , log(log_)
{
}

void TableSnapshot::initSnapshot()
{
    if (snapshot.get())
        return;

    auto * engine_builder = helper->createBuilder();
    engine = KernelUtils::unwrapResult(ffi::builder_build(engine_builder), "builder_build");
    snapshot = KernelUtils::unwrapResult(
        ffi::snapshot(KernelUtils::toDeltaString(helper->getTableLocation()), engine.get()), "snapshot");
    snapshot_version = ffi::version(snapshot.get());

    LOG_TEST(log, "Snapshot version: {}", snapshot_version);
}

ffi::SharedSnapshot * TableSnapshot::getSnapshot()
{
    if (!snapshot.get())
        initSnapshot();
    return snapshot.get();
}

DB::ObjectIterator TableSnapshot::iterate()
{
    initSnapshot();
    return std::make_shared<TableSnapshot::Iterator>(
        engine,
        snapshot,
        helper->getDataPath(),
        getTableSchema(),
        getPartitionColumns(),
        log);
}

const DB::NamesAndTypesList & TableSnapshot::getTableSchema()
{
    if (!table_schema.has_value())
    {
        table_schema = getTableSchemaFromSnapshot(getSnapshot());
        LOG_TEST(log, "Fetched table schema: {}", table_schema->toString());
    }
    return table_schema.value();
}

const DB::NamesAndTypesList & TableSnapshot::getReadSchema()
{
    if (!read_schema.has_value())
    {
        auto * current_snapshot = getSnapshot();
        chassert(engine.get());
        std::tie(read_schema, partition_columns) = getReadSchemaAndPartitionColumnsFromSnapshot(current_snapshot, engine.get());

        LOG_TEST(
            log, "Fetched read schema: {}, partition columns: {}",
            read_schema->toString(), fmt::join(partition_columns.value(), ", "));
    }
    return read_schema.value();
}

const DB::Names & TableSnapshot::getPartitionColumns()
{
    if (!partition_columns.has_value())
    {
        auto * current_snapshot = getSnapshot();
        chassert(engine.get());
        std::tie(read_schema, partition_columns) = getReadSchemaAndPartitionColumnsFromSnapshot(current_snapshot, engine.get());

        LOG_TEST(
            log, "Fetched read schema: {}, partition columns: {}",
            read_schema->toString(), fmt::join(partition_columns.value(), ", "));
    }
    return partition_columns.value();
}

}

#endif
