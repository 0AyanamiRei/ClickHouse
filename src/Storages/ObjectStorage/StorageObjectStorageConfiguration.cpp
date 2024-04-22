#include <Storages/ObjectStorage/StorageObjectStorageConfiguration.h>
#include <Formats/FormatFactory.h>
#include <Common/logger_useful.h>

namespace DB
{
namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
}

void StorageObjectStorageConfiguration::initialize(
    StorageObjectStorageConfiguration & configuration,
    ASTs & engine_args,
    ContextPtr local_context,
    bool with_table_structure)
{
    if (auto named_collection = tryGetNamedCollectionWithOverrides(engine_args, local_context))
        configuration.fromNamedCollection(*named_collection);
    else
        configuration.fromAST(engine_args, local_context, with_table_structure);

    // FIXME: it should be - if (format == "auto" && get_format_from_file)
    if (configuration.format == "auto")
        configuration.format = FormatFactory::instance().tryGetFormatFromFileName(configuration.getPath()).value_or("auto");
    else
        FormatFactory::instance().checkFormatName(configuration.format);

    configuration.check(local_context);
    configuration.initialized = true;
}

void StorageObjectStorageConfiguration::check(ContextPtr) const
{
    FormatFactory::instance().checkFormatName(format);
}

StorageObjectStorageConfiguration::StorageObjectStorageConfiguration(const StorageObjectStorageConfiguration & other)
{
    format = other.format;
    compression_method = other.compression_method;
    structure = other.structure;
}

bool StorageObjectStorageConfiguration::withWildcard() const
{
    static const String PARTITION_ID_WILDCARD = "{_partition_id}";
    return getPath().find(PARTITION_ID_WILDCARD) != String::npos
        || getNamespace().find(PARTITION_ID_WILDCARD) != String::npos;
}

bool StorageObjectStorageConfiguration::isPathWithGlobs() const
{
    return getPath().find_first_of("*?{") != std::string::npos;
}

bool StorageObjectStorageConfiguration::isNamespaceWithGlobs() const
{
    return getNamespace().find_first_of("*?{") != std::string::npos;
}

std::string StorageObjectStorageConfiguration::getPathWithoutGlob() const
{
    return getPath().substr(0, getPath().find_first_of("*?{"));
}

void StorageObjectStorageConfiguration::assertInitialized() const
{
    if (!initialized)
    {
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Configuration was not initialized before usage");
    }
}

}
