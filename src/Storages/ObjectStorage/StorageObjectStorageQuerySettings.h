#pragma once
#include <Interpreters/Context_fwd.h>
#include <Core/Settings.h>
#include <Common/CurrentMetrics.h>

namespace CurrentMetrics
{
    extern const Metric ObjectStorageAzureThreads;
    extern const Metric ObjectStorageAzureThreadsActive;
    extern const Metric ObjectStorageAzureThreadsScheduled;

    extern const Metric ObjectStorageS3Threads;
    extern const Metric ObjectStorageS3ThreadsActive;
    extern const Metric ObjectStorageS3ThreadsScheduled;
}

namespace DB
{

struct StorageObjectStorageSettings
{
    bool truncate_on_insert;
    bool create_new_file_on_insert;
    bool schema_inference_use_cache;
    SchemaInferenceMode schema_inference_mode;
    bool skip_empty_files;
    size_t list_object_keys_size;
    bool throw_on_zero_files_match;
    bool ignore_non_existent_file;
};

struct S3StorageSettings
{
    static StorageObjectStorageSettings create(const Settings & settings)
    {
        return StorageObjectStorageSettings{
            .truncate_on_insert = settings.s3_truncate_on_insert,
            .create_new_file_on_insert = settings.s3_create_new_file_on_insert,
            .schema_inference_use_cache = settings.schema_inference_use_cache_for_s3,
            .schema_inference_mode = settings.schema_inference_mode,
            .skip_empty_files = settings.s3_skip_empty_files,
            .list_object_keys_size = settings.s3_list_object_keys_size,
            .throw_on_zero_files_match = settings.s3_throw_on_zero_files_match,
            .ignore_non_existent_file = settings.s3_ignore_file_doesnt_exist,
        };
    }

    static constexpr auto SCHEMA_CACHE_MAX_ELEMENTS_CONFIG_SETTING = "schema_inference_cache_max_elements_for_s3";

    static CurrentMetrics::Metric ObjectStorageThreads() { return CurrentMetrics::ObjectStorageS3Threads; } /// NOLINT
    static CurrentMetrics::Metric ObjectStorageThreadsActive() { return CurrentMetrics::ObjectStorageS3ThreadsActive; } /// NOLINT
    static CurrentMetrics::Metric ObjectStorageThreadsScheduled() { return CurrentMetrics::ObjectStorageS3ThreadsScheduled; } /// NOLINT
};

struct AzureStorageSettings
{
    static StorageObjectStorageSettings create(const Settings & settings)
    {
        return StorageObjectStorageSettings{
            .truncate_on_insert = settings.azure_truncate_on_insert,
            .create_new_file_on_insert = settings.azure_create_new_file_on_insert,
            .schema_inference_use_cache = settings.schema_inference_use_cache_for_azure,
            .schema_inference_mode = settings.schema_inference_mode,
            .skip_empty_files = settings.s3_skip_empty_files, /// TODO: add setting for azure
            .list_object_keys_size = settings.azure_list_object_keys_size,
            .throw_on_zero_files_match = settings.s3_throw_on_zero_files_match,
            .ignore_non_existent_file = settings.azure_ignore_file_doesnt_exist,
        };
    }

    static constexpr auto SCHEMA_CACHE_MAX_ELEMENTS_CONFIG_SETTING = "schema_inference_cache_max_elements_for_azure";

    static CurrentMetrics::Metric ObjectStorageThreads() { return CurrentMetrics::ObjectStorageAzureThreads; } /// NOLINT
    static CurrentMetrics::Metric ObjectStorageThreadsActive() { return CurrentMetrics::ObjectStorageAzureThreadsActive; } /// NOLINT
    static CurrentMetrics::Metric ObjectStorageThreadsScheduled() { return CurrentMetrics::ObjectStorageAzureThreadsScheduled; } /// NOLINT
};

struct HDFSStorageSettings
{
    static StorageObjectStorageSettings create(const Settings & settings)
    {
        return StorageObjectStorageSettings{
            .truncate_on_insert = settings.hdfs_truncate_on_insert,
            .create_new_file_on_insert = settings.hdfs_create_new_file_on_insert,
            .schema_inference_use_cache = settings.schema_inference_use_cache_for_hdfs,
            .schema_inference_mode = settings.schema_inference_mode,
            .skip_empty_files = settings.hdfs_skip_empty_files, /// TODO: add setting for hdfs
            .list_object_keys_size = settings.s3_list_object_keys_size, /// TODO: add a setting for hdfs
            .throw_on_zero_files_match = settings.s3_throw_on_zero_files_match,
            .ignore_non_existent_file = settings.hdfs_ignore_file_doesnt_exist,
        };
    }

    static constexpr auto SCHEMA_CACHE_MAX_ELEMENTS_CONFIG_SETTING = "schema_inference_cache_max_elements_for_hdfs";

    /// TODO: s3 -> hdfs
    static CurrentMetrics::Metric ObjectStorageThreads() { return CurrentMetrics::ObjectStorageS3Threads; } /// NOLINT
    static CurrentMetrics::Metric ObjectStorageThreadsActive() { return CurrentMetrics::ObjectStorageS3ThreadsActive; } /// NOLINT
    static CurrentMetrics::Metric ObjectStorageThreadsScheduled() { return CurrentMetrics::ObjectStorageS3ThreadsScheduled; } /// NOLINT
};

}
