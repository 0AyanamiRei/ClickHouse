#pragma once

#include "config.h"

#if USE_PARQUET && USE_DELTA_KERNEL_RS

#include <Interpreters/Context_fwd.h>
#include <Core/Types.h>
#include <Storages/ObjectStorage/StorageObjectStorage.h>
#include <Storages/ObjectStorage/StorageObjectStorageSettings.h>
#include <Storages/ObjectStorage/StorageObjectStorageSource.h>
#include <Storages/ObjectStorage/DataLakes/IDataLakeMetadata.h>
#include <Disks/ObjectStorages/IObjectStorage.h>

namespace DeltaLake
{
class TableSnapshot;
}

namespace DB
{
namespace StorageObjectStorageSetting
{
extern const StorageObjectStorageSettingsBool allow_experimental_delta_kernel_rs;
extern const StorageObjectStorageSettingsBool delta_lake_read_schema_same_as_table_schema;
}

class DeltaLakeMetadataDeltaKernel final : public IDataLakeMetadata
{
public:
    using ConfigurationObserverPtr = StorageObjectStorage::ConfigurationObserverPtr;
    static constexpr auto name = "DeltaLake";

    DeltaLakeMetadataDeltaKernel(
        ObjectStoragePtr object_storage_,
        ConfigurationObserverPtr configuration_,
        bool read_schema_same_as_table_schema_);

    DeltaLakeMetadataDeltaKernel(const DeltaLakeMetadataDeltaKernel & other) : log(other.log), table_snapshot(other.table_snapshot) {}

    bool supportsUpdate() const override { return true; }

    bool update(const ContextPtr & context) override;

    std::unique_ptr<IDataLakeMetadata> clone() override {
        return std::make_unique<DeltaLakeMetadataDeltaKernel>(*this);
    }

    NamesAndTypesList getTableSchema() const override;

    DB::ReadFromFormatInfo prepareReadingFromFormat(
        const Strings & requested_columns,
        const DB::StorageSnapshotPtr & storage_snapshot,
        const ContextPtr & context,
        bool supports_subset_of_columns) override;

    bool operator ==(const IDataLakeMetadata &) const override;

    void modifyFormatSettings(FormatSettings & format_settings) const override;

    static DataLakeMetadataPtr create(
        ObjectStoragePtr object_storage,
        ConfigurationObserverPtr configuration,
        ContextPtr, bool)
    {
        auto configuration_ptr = configuration.lock();
        const auto & settings_ref = configuration_ptr->getSettingsRef();
        return std::make_unique<DeltaLakeMetadataDeltaKernel>(
            object_storage,
            configuration,
            settings_ref[StorageObjectStorageSetting::delta_lake_read_schema_same_as_table_schema]);
    }

    ObjectIterator iterate(
        const ActionsDAG * filter_dag,
        FileProgressCallback callback,
        size_t list_batch_size) const override;

private:
    const LoggerPtr log;
    const std::shared_ptr<DeltaLake::TableSnapshot> table_snapshot;
};

}

#endif
